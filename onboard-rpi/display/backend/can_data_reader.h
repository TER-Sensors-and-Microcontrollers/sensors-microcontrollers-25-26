#ifndef CAN_DATA_READER_H
#define CAN_DATA_READER_H

#include <string>
#include <cmath> 

// Define internal ID for the only sensor used (must match the .cpp file)
enum SensorID {
    ID_ENGINE_TEMP = 1
};

// Function declarations exposed to the Qt application
void initialize_database();
void simulate_can_loop(); // For simulation setup
float get_sensor_value(SensorID internal_id);

// Constant declaration
extern const std::string DB_FILE_PATH;

#endif // CAN_DATA_READER_H