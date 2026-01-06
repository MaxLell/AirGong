#ifndef MESSAGE_DEFINITIONS_H
#define MESSAGE_DEFINITIONS_H

#include <time.h>
#include "custom_types.h"

// =============================
// Time Sync Message Structures
// =============================

typedef struct
{
    // Empty - just a request
} msg_time_get_request_t;

typedef struct
{
    time_t timestamp;   // Unix timestamp
    struct tm timeinfo; // Broken-down time
    bool time_valid;    // Whether time has been synchronized
} msg_time_get_response_t;

// =============================
// WiFi Credentials Message Structures
// =============================

#define WIFI_SSID_MAX_LENGTH     32
#define WIFI_PASSWORD_MAX_LENGTH 64

typedef struct
{
    char ssid[WIFI_SSID_MAX_LENGTH];
    char password[WIFI_PASSWORD_MAX_LENGTH];
} msg_wifi_set_credentials_t;

typedef struct
{
    // Empty - just a request
} msg_wifi_get_credentials_t;

typedef struct
{
    char ssid[WIFI_SSID_MAX_LENGTH];
    char password[WIFI_PASSWORD_MAX_LENGTH];
    bool has_credentials;
} msg_wifi_credentials_response_t;

typedef enum
{
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED
} wifi_connection_status_e;

typedef struct
{
    wifi_connection_status_e status;
    char ssid[WIFI_SSID_MAX_LENGTH];
    int rssi; // Signal strength
} msg_wifi_connection_status_t;

#endif // MESSAGE_DEFINITIONS_H
