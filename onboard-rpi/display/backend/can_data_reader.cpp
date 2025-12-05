#include <iostream>
#include <iomanip>
#include <map>
#include <cmath> 
#include <unistd.h> 
#include <cstring>  
#include <stdint.h> 
#include <fstream>
#include <ctime>
#include <chrono>
#include <sstream>
#include <sqlite3.h> // Required for SQLite database operations

// --- START LINUX CAN-SPECIFIC CODE BLOCK ---
#ifndef __APPLE__ 
// Headers for SocketCAN implementation (required on Linux/RPi)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#else
// Define necessary structures/constants for simulation on macOS 
struct can_frame {
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t data[8];
};
#define CAN_EFF_MASK 0x1FFFFFFF
#endif
// --- END LINUX CAN-SPECIFIC CODE BLOCK ---


using namespace std;

// --- CONSTANTS AND DATA STRUCTURES (TASK 2) ---

// Define internal ID for the only sensor used
enum SensorID {
    ID_ENGINE_TEMP = 1
};

// Map CAN ID to internal Sensor ID
const map<uint32_t, SensorID> CAN_ID_MAPPING = {
    {0x1A0, ID_ENGINE_TEMP} // CAN ID 0x1A0 maps to Engine Temp
};

// Global data store using a map for current sensor values
map<SensorID, float> g_sensor_values;
const string DB_FILE_PATH = "can_log.db"; // SQLite database file path

// --- ENDIANNESS CONVERSION UTILITY (Task 2 - UNUSED) ---
uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

// --- DECODER AND GETTER FUNCTIONS (TASK 2) ---

/**
 * Decodes the raw CAN frame payload into meaningful sensor values.
 *
 * frame: A pointer to the raw received CAN frame.
 */
void process_can_frame(const struct can_frame *frame) {
    uint32_t can_id = frame->can_id & CAN_EFF_MASK;

    auto it = CAN_ID_MAPPING.find(can_id);
    if (it == CAN_ID_MAPPING.end()) {
        cout << " (ID not mapped, ignoring)";
        return;
    }

    SensorID internal_id = it->second;
    float decoded_value = nanf("");

    switch (internal_id) {
        case ID_ENGINE_TEMP: {
            // Protocol: 16-bit signed integer (int16_t) in LE, scaled by 0.1
            if (frame->can_dlc >= 2) {
                uint16_t raw_temp_uint;
                // Safely copy data from the payload
                memcpy(&raw_temp_uint, frame->data, 2); 
                
                // Scale the raw value
                int16_t raw_temp_int = (int16_t)raw_temp_uint;
                decoded_value = (float)raw_temp_int / 10.0f; 
                #ifndef __APPLE__
                cout << " -> DECODED: Temp = " << fixed << setprecision(1) << decoded_value << " C";
                #endif
            }
            break;
        }
        
        default:
             break;
    }

    if (!isnan(decoded_value)) {
        g_sensor_values[internal_id] = decoded_value;
    }
}

/**
 * Getter function to retrieve the latest sensor value.
 */
float get_sensor_value(SensorID internal_id) {
    auto it = g_sensor_values.find(internal_id);
    if (it != g_sensor_values.end()) {
        return it->second;
    }
    return nanf(""); 
}

// --- SQLITE LOGGING FUNCTIONS (TASK 3) ---

/**
 * Initializes the SQLite database and creates the logging table if it does not exist.
 */
void initialize_database() {
    sqlite3 *db;
    char *err_msg = 0;
    
    // Open the database file. If it doesn't exist, it is created.
    int rc = sqlite3_open(DB_FILE_PATH.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        // Since this might be running in a path without write permission, just log the error
        cerr << "SQL ERROR: Cannot open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    // SQL statement to create the table, only including EngineTemp
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS sensor_data("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Timestamp TEXT NOT NULL, "
        "EngineTemp_C REAL"
        ");";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {
        cerr << "SQL ERROR: Failed to create table: " << err_msg << endl;
        sqlite3_free(err_msg);
    } else {
        #ifndef __APPLE__
        cout << "Database initialized and table is ready." << endl;
        #endif
    }

    sqlite3_close(db);
}

/**
 * Logs the current state of the Engine Temperature to the SQLite database.
 */
void log_data_to_sd_card_sqlite() {
    // We disable logging completely during local macOS simulation to avoid
    // issues with missing libraries or file path conflicts.
    #ifndef __APPLE__ 
        sqlite3 *db;
        char *err_msg = 0;
        
        // 1. Open Connection
        int rc = sqlite3_open(DB_FILE_PATH.c_str(), &db);
        if (rc != SQLITE_OK) {
            cerr << "SQL ERROR: Cannot open database for logging: " << sqlite3_errmsg(db) << endl;
            return;
        }

        // 2. Get current timestamp as string
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        char timestamp_buffer[80];
        strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        // 3. Prepare SQL INSERT statement (Temperature only)
        stringstream ss;
        ss << "INSERT INTO sensor_data (Timestamp, EngineTemp_C) VALUES ("
        << "'" << timestamp_buffer << "', "
        << fixed << setprecision(2) << get_sensor_value(ID_ENGINE_TEMP)
        << ");";
        
        string sql_insert = ss.str();
        
        // 4. Execute the statement
        rc = sqlite3_exec(db, sql_insert.c_str(), 0, 0, &err_msg);

        if (rc != SQLITE_OK ) {
            cerr << "SQL ERROR: Data insert failed: " << err_msg << endl;
            sqlite3_free(err_msg);
        }

        // 5. Close Connection
        sqlite3_close(db);
    #endif // __APPLE__
}


// --- SIMULATION STUB (Used only for CLI/RPi testing) ---

void simulate_can_loop() {
    // This loop is for testing on the RPi/CLI only and should NOT be called by the GUI
    #ifndef __APPLE__ 
        cout << "\n--- SIMULATION MODE ACTIVE (Decoding & SQLite Logging for Temp Sensor) ---" << endl;
        cout << "Log data will be stored in SQLite database: " << DB_FILE_PATH << endl;
        
        // Initialize the database and table first
        initialize_database(); 
        
        // Test Case: Engine Temperature (ID 0x1A0, 16-bit scaled int)
        // Target Value: 85.5 C (raw value 855 or 0x0357 in LE)
        struct can_frame temp_frame = {0};
        temp_frame.can_id = 0x1A0;
        temp_frame.can_dlc = 2;
        temp_frame.data[0] = 0x57; // Low byte
        temp_frame.data[1] = 0x03; // High byte

        cout << "--------------------------------------------------------------------------------" << endl;
        cout << "| CAN ID    | DLC | Data (Hex)                              | Decoded Value (Task 2) |" << endl;
        cout << "--------------------------------------------------------------------------------" << endl;

        int count = 0;
        while (count < 10) { 
            // 1. Process the incoming temperature frame
            cout << "| 0x" << hex << setw(8) << setfill('0') << temp_frame.can_id << dec;
            cout << "  |  " << (int)temp_frame.can_dlc << "  | 57 03                            |";
            process_can_frame(&temp_frame);
            cout << endl;
            
            // 2. Log the current data set
            log_data_to_sd_card_sqlite();

            // 3. Print status and wait
            cout << ">> LOGGED: Temp=" << get_sensor_value(ID_ENGINE_TEMP) << "\n" << endl;

            usleep(500000); // 500ms delay
            count++;
        }

        cout << "--- SIMULATION ENDED. Check " << DB_FILE_PATH << " for data. ---" << endl;
    #endif // __APPLE__
}

// --- MAIN APPLICATION LOGIC (Entry Point) ---

/**
 * The main() function in this file is only included when building the CLI executable.
 * It is EXCLUDED when building the Qt GUI application, which uses the main() in main.cpp.
 */
#ifndef QT_GUI_LIB 
int main(int argc, char **argv) {
    if (argc == 2) {
        cout << "CAN Data Reader starting..." << endl;
        cout << "!! WARNING: Running SQLite Simulation on CLI. !!" << endl;
        simulate_can_loop();
    } else {
        simulate_can_loop();
    }
    
    return 0;
}
#endif // QT_GUI_LIB