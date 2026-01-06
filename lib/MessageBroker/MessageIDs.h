#ifndef MESSAGEIDS_H_
#define MESSAGEIDS_H_

/**
 * Message Topics
 */
typedef enum
{
    E_TOPIC_FIRST_TOPIC = 0U, // First Topic - DO NOT USE (Only for boundary checks)

    // Test Messages and System Messages
    MSG_0001, // Chaos Elephant (Placeholder)
    MSG_0002, // Tickly Giraffe (Placeholder)
    MSG_0003, // Set Logging Level on Module
    MSG_0004, // Request System Status

    // Messages for the Modules

    // Time Sync Messages
    MSG_0100, // Request current time
    MSG_0101, // Response with current time

    // WiFi Credentials Messages
    MSG_0200, // Set WiFi SSID and Password
    MSG_0201, // Get current WiFi credentials
    MSG_0202, // Response with WiFi credentials
    MSG_0203, // WiFi connection status update

    E_TOPIC_LAST_TOPIC // Last Topic - DO NOT USE (Only for boundary checks)
} msg_id_e;

#endif /* MESSAGEIDS_H_ */