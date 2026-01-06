#ifndef MESSAGE_DEFINITIONS_H
#define MESSAGE_DEFINITIONS_H

#include <time.h>
#include "custom_types.h"

// =============================
// System Message Structures
// =============================

#define MODULE_NAME_MAX_LENGTH 32

typedef enum
{
    MODULE_APPCONTROL = 0,
    MODULE_MP3PLAYER,
    MODULE_TIMESYNC,
    MODULE_WIFIMANAGER,
    MODULE_CONSOLE,
    MODULE_ALL // Special value for all modules
} module_id_e;

typedef struct
{
    module_id_e module_id;                    // Module to configure
    bool enabled;                             // Enable or disable logging
    char module_name[MODULE_NAME_MAX_LENGTH]; // Module name as string (alternative to ID)
} msg_set_logging_t;

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

// =============================
// MP3 Player Message Structures
// =============================

typedef enum
{
    MP3_MODE_LOOP = 1,        // Loop all songs
    MP3_MODE_SINGLE_LOOP = 2, // Loop single song
    MP3_MODE_FOLDER_LOOP = 3, // Loop folder
    MP3_MODE_RANDOM = 4,      // Random playback
    MP3_MODE_SINGLE_SHOT = 5  // Play single song once
} mp3_play_mode_e;

typedef struct
{
    u8 volume; // Volume level (0-31)
} msg_mp3_set_volume_t;

typedef struct
{
    mp3_play_mode_e mode; // Play mode
} msg_mp3_set_playmode_t;

typedef struct
{
    u16 song_index; // Song index to play
} msg_mp3_play_song_t;

typedef struct
{
    bool success;   // Whether command was successful
    int error_code; // Error code if not successful
} msg_mp3_command_response_t;

// =============================
// Application Control Message Structures
// =============================

typedef struct
{
    u8 hour;        // Hour (0-23)
    u8 minute;      // Minute (0-59)
    u16 song_index; // Song index to play
} msg_schedule_add_t;

typedef struct
{
    int schedule_id; // ID of schedule to remove
} msg_schedule_remove_t;

typedef struct
{
    bool enabled; // Enable or disable scheduling
} msg_schedule_enable_t;

typedef struct
{
    bool success;    // Whether command was successful
    int schedule_id; // Schedule ID (-1 if not applicable)
} msg_schedule_response_t;

typedef struct
{
    int schedule_id;
    u8 hour;
    u8 minute;
    u16 song_index;
} schedule_info_t;

typedef struct
{
    u8 count;                      // Number of schedules
    schedule_info_t schedules[20]; // Schedule entries
} msg_schedule_list_t;

#endif // MESSAGE_DEFINITIONS_H
