#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

// SocketCAN headers-mac compatiblity with linux
#include <linux/can.h>
#include <linux/can/raw.h>

/**
 * Reads and prints CAN frames from a specified SocketCAN interface.
 *
 * Usage: ./can_reader <interface_name>
 * Example: ./can_reader vcan0
 *
 */
int main(int argc, char **argv) {
    int s; /* socket file descriptor */
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    char *ifname;

    // 1. Argument Handling
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface_name>\n", argv[0]);
        fprintf(stderr, "Example: %s vcan0\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    ifname = argv[1];

    printf("Starting CAN reader on interface %s...\n", ifname);

    // 2. Create the CAN socket
    // PF_CAN (Protocol Family CAN), SOCK_RAW (Raw Socket), CAN_RAW (Raw CAN Protocol)
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Error creating socket");
        return 1;
    }

    // 3. Get the interface index
    // This connects the socket to a specific network interface
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting interface index (SIOCGIFINDEX)");
        close(s);
        return 1;
    }

    // 4. Bind the socket to the interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error binding socket to interface");
        close(s);
        return 1;
    }

    // Print header for incoming data
    printf("Listening for CAN frames (Ctrl+C to stop)...\n");
    printf("-----------------------------------------------------------\n");
    printf("| CAN ID    | DLC | Data (Hex)                              |\n");
    printf("-----------------------------------------------------------\n");


    //  Continuous Read Loop
    while (1) {
        // Read exactly one CAN frame from the socket
        ssize_t nbytes = read(s, &frame, sizeof(frame));

        if (nbytes < 0) {
            perror("Error reading CAN frame");
            break; // Exit loop on read error
        }

        // Check if the correct amount of data was read (a complete CAN frame)
        if (nbytes == sizeof(frame)) {
            // Print the CAN ID (formatted as 8-digit hex)
            printf("| 0x%08X  |  %d  | ", frame.can_id, frame.can_dlc);

            // Print the data bytes (up to 8 bytes)
            for (int i = 0; i < frame.can_dlc; i++) {
                printf("%02X ", frame.data[i]);
            }
            // Fill remaining space if DLC < 8 for clean alignment
            for (int i = frame.can_dlc; i < 8; i++) {
                printf("   ");
            }
            printf("|\n");
        }
    }

    // Cleanup
    close(s);
    printf("\nCAN reader shut down.\n");

    return 0;
}
