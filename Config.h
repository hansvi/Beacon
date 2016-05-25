#ifndef CONFIG_H_
#define CONFIG_H_

// MAC address, check on ethernet shield
#define MAC_ADDRESS 0x90, 0xA2, 0xDA, 0x0F, 0x64, 0x55 
// IP address, may need to change depending on network
#define IP_ADDRESS 192, 168, 15, 30
// TCP port of the http server
#define HTTP_PORT 80
// HTTP request max line length
#define HTTP_REQ_BUF_SZ        180
//Max filename size for http requests
#define HTTP_REQ_FILENAME_SZ   150

// Number of beacons
#define BEACON_COUNT 9
// Maximum message length in characters
#define BEACON_MESSAGE_LENGTH 44

// Physical limitations
#define NUM_ANALOG_CHANNELS 16
#define NUM_TEMPERATURE_CHANNELS 8

#define LOGLINE_SIZE ((NUM_ANALOG_CHANNELS*5) + (NUM_TEMPERATURE_CHANNELS*6) + 15 )

// Log interval in seconds
#define LOG_INTERVAL 300

#endif
