/**
 * MIT License
 *
 * Copyright (c) <2025> <Max Koell (maxkoell@proton.me)>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file Console.c
 * @brief Console module that initializes the CLI and registers demo commands for Arduino/RTOS environment.
 *
 * This file provides console functionality using the CLI library adapted for
 * Arduino framework with RTOS task support.
 */

#include "Console.h"
#include "Cli.h"
#include "MessageBroker.h"
#include "MessageDefinitions.h"
#include "custom_assert.h"
#include "custom_types.h"

// Include Arduino Serial for I/O
#include <Arduino.h>

// ###########################################################################
// # Private function declarations
// ###########################################################################
static int prv_console_put_char(char in_char);
static char prv_console_get_char(void);

// System Commands
static int prv_cmd_system_info(int argc, char* argv[], void* context);
static int prv_cmd_reset_system(int argc, char* argv[], void* context);

// Message Broker Test commands
static void prv_msg_broker_callback(const msg_t* const message);
static int prv_cmd_msgbroker_can_subscribe_and_publish(int argc, char* argv[], void* context);

// Time Sync Commands
static void prv_time_callback(const msg_t* const message);
static int prv_cmd_time_get(int argc, char* argv[], void* context);

// WiFi Commands
static void prv_wifi_callback(const msg_t* const message);
static int prv_cmd_wifi_set(int argc, char* argv[], void* context);
static int prv_cmd_wifi_get(int argc, char* argv[], void* context);

// MP3 Player Commands
static void prv_mp3_callback(const msg_t* const message);
static int prv_cmd_mp3_volume(int argc, char* argv[], void* context);
static int prv_cmd_mp3_mode(int argc, char* argv[], void* context);
static int prv_cmd_mp3_play(int argc, char* argv[], void* context);
static int prv_cmd_mp3_volume_up(int argc, char* argv[], void* context);
static int prv_cmd_mp3_volume_down(int argc, char* argv[], void* context);
static int prv_cmd_mp3_next(int argc, char* argv[], void* context);
static int prv_cmd_mp3_previous(int argc, char* argv[], void* context);
static int prv_cmd_mp3_pause(int argc, char* argv[], void* context);

// Application Control Commands
static void prv_schedule_callback(const msg_t* const message);
static int prv_cmd_schedule_add(int argc, char* argv[], void* context);
static int prv_cmd_schedule_remove(int argc, char* argv[], void* context);
static int prv_cmd_schedule_list(int argc, char* argv[], void* context);
static int prv_cmd_schedule_clear(int argc, char* argv[], void* context);
static int prv_cmd_schedule_enable(int argc, char* argv[], void* context);

// Logging Commands
static int prv_cmd_log(int argc, char* argv[], void* context);

// ###########################################################################
// # Private Variables
// ###########################################################################

static bool is_initialized = false;

// embedded cli object - contains all data. This memory is to be managed by the user
static cli_cfg_t g_cli_cfg = {0};

/**
 * 'command name' - 'command handler' - 'pointer to context' - 'help string'
 *
 * - The command name is the ... name of the command
 * - The 'command handler' is the function pointer to the function that is to be called, when the command was successfully entered
 * - The 'pointer to context' is the context which the user can provide to his handler function - this can also be NULL
 * - The 'help string' is the string that is printed when the help command is executed. (Have a look at the Readme.md file for an example)
 */
static cli_binding_t cli_bindings[] = {
    // System Commands
    {"system_info", prv_cmd_system_info, NULL, "Show system information"},
    {"restart", prv_cmd_reset_system, NULL, "Restart the system"},

    // Message Broker Test Commands
    {"msgbroker_test", prv_cmd_msgbroker_can_subscribe_and_publish, NULL, "Test Message Broker subscribe and publish"},

    // WiFi Commands
    {"wifi_set", prv_cmd_wifi_set, NULL, "Set WiFi credentials: wifi_set <ssid> <password> (use quotes for spaces)"},
    {"wifi_get", prv_cmd_wifi_get, NULL, "Get current WiFi credentials"},

    // MP3 Player Commands
    {"speaker_volume", prv_cmd_mp3_volume, NULL, "Set volume: speaker_volume <0-31>"},
    {"speaker_mode", prv_cmd_mp3_mode, NULL,
     "Set play mode: speaker_mode <1-5> (1=loop, 2=single loop, 3=folder loop, 4=random, 5=single shot)"},
    {"speaker_play", prv_cmd_mp3_play, NULL, "Play song by index: speaker_play <index>"},
    {"speaker_volume_up", prv_cmd_mp3_volume_up, NULL, "Increase volume"},
    {"speaker_volume_down", prv_cmd_mp3_volume_down, NULL, "Decrease volume"},
    {"speaker_next", prv_cmd_mp3_next, NULL, "Next song"},
    {"speaker_previous", prv_cmd_mp3_previous, NULL, "Previous song"},
    {"speaker_pause", prv_cmd_mp3_pause, NULL, "Pause or play"},

    // Application Control Commands
    {"schedule_add", prv_cmd_schedule_add, NULL, "Add schedule: schedule_add <hour> <minute> <song_index> <weekdays>"},
    {"schedule_remove", prv_cmd_schedule_remove, NULL, "Remove schedule: schedule_remove <id>"},
    {"schedule_list", prv_cmd_schedule_list, NULL, "List all schedules"},
    {"schedule_clear", prv_cmd_schedule_clear, NULL, "Clear all schedules"},
    {"schedule_enable", prv_cmd_schedule_enable, NULL, "Enable/disable scheduling: schedule_enable <0|1>"},

    // Logging Commands
    {"log", prv_cmd_log, NULL, "Enable/disable logging: log <on|off> <module_name>"},

};

// ###########################################################################
// # Public function implementations
// ###########################################################################

void console_init(void)
{
    ASSERT(!is_initialized);

    // Initialize Serial communication
    Serial.begin(115200);

    // Subscribe to message broker responses
    messagebroker_subscribe(MSG_0202, prv_wifi_callback);
    messagebroker_subscribe(MSG_0203, prv_wifi_callback);
    messagebroker_subscribe(MSG_0308, prv_mp3_callback);      // MP3 command responses
    messagebroker_subscribe(MSG_0405, prv_schedule_callback); // Schedule responses
    messagebroker_subscribe(MSG_0406, prv_schedule_callback); // Schedule list

    cli_init(&g_cli_cfg, prv_console_put_char);

    // Register all commands
    for (size_t i = 0; i < CLI_GET_ARRAY_SIZE(cli_bindings); i++)
    {
        cli_register(&cli_bindings[i]);
    }

    is_initialized = true;
}

void console_run(void)
{
    ASSERT(is_initialized);

    // Check if there are characters available from Serial
    if (Serial.available() > 0)
    {
        // Get a character entered by the user
        char c = prv_console_get_char();

        // Add the character to a queue and process it
        cli_receive_and_process(c);
    }
}

// ###########################################################################
// # Private function implementations
// ###########################################################################

// ============================
// = Console I/O (Arduino Serial)
// ============================

static int prv_console_put_char(char in_char)
{
    Serial.write(in_char);
    return 1; // Success
}

static char prv_console_get_char(void)
{
    if (Serial.available() > 0)
    {
        return (char)Serial.read();
    }
    return 0; // No character available
}

// ============================
// = Commands
// ============================

static int prv_cmd_system_info(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    // Basic system info
    cli_print("* Uptime: %lu ms", millis());
    cli_print("* Temperature: %.1f°C", temperatureRead());
    return CLI_OK_STATUS;
}

static int prv_cmd_reset_system(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    cli_print("Restarting system in ");
    for (int i = 3; i > 0; i--)
    {
        cli_print("%d... ", i);
        delay(1000);
    }
    cli_print("\n");
    ESP.restart();

    return CLI_OK_STATUS;
}

static void prv_msg_broker_callback(const msg_t* const message)
{
    switch (message->msg_id)
    {
        case MSG_0001:
            cli_print("Message was received\n...");
            cli_print("Received message ID: %d, Size: %d", message->msg_id, message->data_size);
            cli_print("Message Content: %s", message->data_bytes);
            break;

        default: break;
    }
}

static int prv_cmd_msgbroker_can_subscribe_and_publish(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    static bool run_once = true;

    if (run_once)
    {
        // Subscribe to a test message
        cli_print("Subscribed to MSG_0001 \n... \nNow publishing a test message. \n...");
        messagebroker_subscribe(MSG_0001, prv_msg_broker_callback);
        run_once = false;
    }

    // Publish a test message
    msg_t test_msg;
    test_msg.msg_id = MSG_0001;
    const char* test_data = "The elephant has been tickled!";
    test_msg.data_size = strlen(test_data) + 1; // Including null terminator
    test_msg.data_bytes = (u8*)test_data;

    messagebroker_publish(&test_msg);

    return CLI_OK_STATUS;
}

// ============================
// = WiFi Commands
// ============================

static void prv_wifi_callback(const msg_t* const message)
{
    switch (message->msg_id)
    {
        case MSG_0202:
        {
            msg_wifi_credentials_response_t* response = (msg_wifi_credentials_response_t*)message->data_bytes;

            if (response->has_credentials)
            {
                cli_print("WiFi Credentials:");
                cli_print("  SSID: %s", response->ssid);
                cli_print("  Password: %s", response->password);
            }
            else
            {
                cli_print("No WiFi credentials stored.");
            }
            break;
        }

        case MSG_0203:
        {
            msg_wifi_connection_status_t* status = (msg_wifi_connection_status_t*)message->data_bytes;

            switch (status->status)
            {
                case WIFI_STATUS_DISCONNECTED: cli_print("WiFi Status: Disconnected"); break;
                case WIFI_STATUS_CONNECTING: cli_print("WiFi Status: Connecting to %s...", status->ssid); break;
                case WIFI_STATUS_CONNECTED:
                    cli_print("WiFi Status: Connected to %s (RSSI: %d dBm)", status->ssid, status->rssi);
                    break;
                case WIFI_STATUS_FAILED: cli_print("WiFi Status: Connection failed"); break;
            }
            break;
        }

        default: break;
    }
}

static int prv_cmd_wifi_set(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 3)
    {
        cli_print("Usage: wifi_set <ssid> <password>");
        cli_print("       Use quotes for SSIDs/passwords with spaces, e.g.: wifi_set \"My Network\" password123");
        return CLI_FAIL_STATUS;
    }

    msg_wifi_set_credentials_t credentials;
    strncpy(credentials.ssid, argv[1], WIFI_SSID_MAX_LENGTH - 1);
    credentials.ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';

    strncpy(credentials.password, argv[2], WIFI_PASSWORD_MAX_LENGTH - 1);
    credentials.password[WIFI_PASSWORD_MAX_LENGTH - 1] = '\0';

    msg_t msg;
    msg.msg_id = MSG_0200;
    msg.data_size = sizeof(msg_wifi_set_credentials_t);
    msg.data_bytes = (u8*)&credentials;

    cli_print("Setting WiFi credentials...");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_wifi_get(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    // Request WiFi credentials
    msg_wifi_get_credentials_t request;
    msg_t msg;
    msg.msg_id = MSG_0201;
    msg.data_size = sizeof(msg_wifi_get_credentials_t);
    msg.data_bytes = (u8*)&request;

    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

// ============================
// = MP3 Player Commands
// ============================

static void prv_mp3_callback(const msg_t* const message)
{
    switch (message->msg_id)
    {
        case MSG_0308:
        {
            msg_mp3_command_response_t* response = (msg_mp3_command_response_t*)message->data_bytes;

            if (response->success)
            {
                cli_print("MP3 Command successful");
            }
            else
            {
                cli_print("MP3 Command failed with error code: %d", response->error_code);
            }
            break;
        }

        default: break;
    }
}

static int prv_cmd_mp3_volume(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 2)
    {
        cli_print("Usage: speaker_volume <volume> (0-31)");
        return CLI_FAIL_STATUS;
    }

    int volume = atoi(argv[1]);
    if (volume < 0 || volume > 31)
    {
        cli_print("Volume must be between 0 and 31");
        return CLI_FAIL_STATUS;
    }

    msg_mp3_set_volume_t vol_cmd;
    vol_cmd.volume = (u8)volume;

    msg_t msg;
    msg.msg_id = MSG_0300;
    msg.data_size = sizeof(msg_mp3_set_volume_t);
    msg.data_bytes = (u8*)&vol_cmd;

    cli_print("Setting volume to: %d", volume);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_mode(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 2)
    {
        cli_print("Usage: speaker_mode <mode>");
        cli_print("  1 - Loop mode");
        cli_print("  2 - Single song loop mode");
        cli_print("  3 - Folder loop mode");
        cli_print("  4 - Random mode");
        cli_print("  5 - Single song mode");
        return CLI_FAIL_STATUS;
    }

    int mode = atoi(argv[1]);
    if (mode < 1 || mode > 5)
    {
        cli_print("Mode must be between 1 and 5");
        return CLI_FAIL_STATUS;
    }

    msg_mp3_set_playmode_t mode_cmd;
    mode_cmd.mode = (mp3_play_mode_e)mode;

    msg_t msg;
    msg.msg_id = MSG_0301;
    msg.data_size = sizeof(msg_mp3_set_playmode_t);
    msg.data_bytes = (u8*)&mode_cmd;

    const char* mode_names[] = {"", "Loop", "Single Loop", "Folder Loop", "Random", "Single Shot"};
    cli_print("Setting play mode to: %s", mode_names[mode]);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_play(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 2)
    {
        cli_print("Usage: speaker_play <song_index>");
        return CLI_FAIL_STATUS;
    }

    int song_index = atoi(argv[1]);
    if (song_index < 1)
    {
        cli_print("Song index must be >= 1");
        return CLI_FAIL_STATUS;
    }

    msg_mp3_play_song_t play_cmd;
    play_cmd.song_index = (u16)song_index;

    msg_t msg;
    msg.msg_id = MSG_0302;
    msg.data_size = sizeof(msg_mp3_play_song_t);
    msg.data_bytes = (u8*)&play_cmd;

    cli_print("Playing song: %d", song_index);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_volume_up(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0303;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Volume up");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_volume_down(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0304;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Volume down");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_next(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0305;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Next song");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_previous(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0306;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Previous song");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_mp3_pause(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0307;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Pause or Play");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

// ============================
// = Application Control Commands
// ============================

static void prv_schedule_callback(const msg_t* const message)
{
    switch (message->msg_id)
    {
        case MSG_0405:
        {
            msg_schedule_response_t* response = (msg_schedule_response_t*)message->data_bytes;

            if (response->success)
            {
                if (response->schedule_id >= 0)
                {
                    cli_print("Schedule operation successful (ID: %d)", response->schedule_id);
                }
                else
                {
                    cli_print("Schedule operation successful");
                }
            }
            else
            {
                cli_print("Schedule operation failed");
            }
            break;
        }

        case MSG_0406:
        {
            msg_schedule_list_t* list = (msg_schedule_list_t*)message->data_bytes;

            if (list->count == 0)
            {
                cli_print("No schedules configured");
            }
            else
            {
                cli_print("Configured schedules:");
                for (int i = 0; i < list->count; i++)
                {
                    // Build weekday string
                    char weekday_str[32] = "";
                    bool first = true;

                    if (list->schedules[i].weekday_mask == 0x7F)
                    {
                        strcpy(weekday_str, "All days");
                    }
                    else
                    {
                        if (list->schedules[i].weekday_mask & (1 << 0))
                        {
                            strcat(weekday_str, first ? "Mo" : ",Mo");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 1))
                        {
                            strcat(weekday_str, first ? "Tue" : ",Tue");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 2))
                        {
                            strcat(weekday_str, first ? "Wed" : ",Wed");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 3))
                        {
                            strcat(weekday_str, first ? "Thu" : ",Thu");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 4))
                        {
                            strcat(weekday_str, first ? "Fri" : ",Fri");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 5))
                        {
                            strcat(weekday_str, first ? "Sat" : ",Sat");
                            first = false;
                        }
                        if (list->schedules[i].weekday_mask & (1 << 6))
                        {
                            strcat(weekday_str, first ? "Sun" : ",Sun");
                            first = false;
                        }
                    }

                    cli_print("  [%d] %02d:%02d -> Song %d (%s)", list->schedules[i].schedule_id,
                              list->schedules[i].hour, list->schedules[i].minute, list->schedules[i].song_index,
                              weekday_str);
                }
            }
            break;
        }

        default: break;
    }
}

static int prv_cmd_schedule_add(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 5)
    {
        cli_print("Usage: schedule_add <hour> <minute> <song_index> <weekdays>");
        cli_print("  hour: 0-23");
        cli_print("  minute: 0-59");
        cli_print("  song_index: 1-9999");
        cli_print("  weekdays: Comma-separated list or '*' for all days");
        cli_print("    Valid days: Mo, Tue, Wed, Thu, Fri, Sat, Sun");
        cli_print("    Examples: 'Mo,Wed,Fri' or '*' or 'Mo,Tue,Wed,Thu,Fri'");
        return CLI_FAIL_STATUS;
    }

    int hour = atoi(argv[1]);
    int minute = atoi(argv[2]);
    int song_index = atoi(argv[3]);

    if (hour < 0 || hour > 23)
    {
        cli_print("Hour must be between 0 and 23");
        return CLI_FAIL_STATUS;
    }

    if (minute < 0 || minute > 59)
    {
        cli_print("Minute must be between 0 and 59");
        return CLI_FAIL_STATUS;
    }

    if (song_index < 1)
    {
        cli_print("Song index must be >= 1");
        return CLI_FAIL_STATUS;
    }

    // Parse weekdays
    u8 weekday_mask = 0;

    // Check if all days ('*')
    if (strcmp(argv[4], "*") == 0)
    {
        weekday_mask = 0x7F; // All 7 days (bits 0-6)
    }
    else
    {
        // Parse comma-separated weekday list
        char* weekdays_copy = argv[4]; // Work with the original string
        char* token = strtok(weekdays_copy, ",");

        while (token != NULL)
        {
            // Trim leading spaces
            while (*token == ' ')
            {
                token++;
            }

            // Parse weekday
            if (strcmp(token, "Mo") == 0 || strcmp(token, "mon") == 0)
            {
                weekday_mask |= (1 << 0); // Monday = bit 0
            }
            else if (strcmp(token, "Tue") == 0 || strcmp(token, "tue") == 0)
            {
                weekday_mask |= (1 << 1); // Tuesday = bit 1
            }
            else if (strcmp(token, "Wed") == 0 || strcmp(token, "wed") == 0)
            {
                weekday_mask |= (1 << 2); // Wednesday = bit 2
            }
            else if (strcmp(token, "Thu") == 0 || strcmp(token, "thu") == 0)
            {
                weekday_mask |= (1 << 3); // Thursday = bit 3
            }
            else if (strcmp(token, "Fri") == 0 || strcmp(token, "fri") == 0)
            {
                weekday_mask |= (1 << 4); // Friday = bit 4
            }
            else if (strcmp(token, "Sat") == 0 || strcmp(token, "sat") == 0)
            {
                weekday_mask |= (1 << 5); // Saturday = bit 5
            }
            else if (strcmp(token, "Sun") == 0 || strcmp(token, "sun") == 0)
            {
                weekday_mask |= (1 << 6); // Sunday = bit 6
            }
            else
            {
                cli_print("Invalid weekday: %s", token);
                cli_print("Valid days: Mo, Tue, Wed, Thu, Fri, Sat, Sun");
                return CLI_FAIL_STATUS;
            }

            token = strtok(NULL, ",");
        }
    }

    if (weekday_mask == 0)
    {
        cli_print("No valid weekdays specified");
        return CLI_FAIL_STATUS;
    }

    msg_schedule_add_t schedule;
    schedule.hour = (u8)hour;
    schedule.minute = (u8)minute;
    schedule.song_index = (u16)song_index;
    schedule.weekday_mask = weekday_mask;

    msg_t msg;
    msg.msg_id = MSG_0400;
    msg.data_size = sizeof(msg_schedule_add_t);
    msg.data_bytes = (u8*)&schedule;

    cli_print("Adding schedule: %02d:%02d -> Song %d (weekday mask: 0x%02X)", hour, minute, song_index, weekday_mask);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_schedule_remove(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 2)
    {
        cli_print("Usage: schedule_remove <id>");
        return CLI_FAIL_STATUS;
    }

    int schedule_id = atoi(argv[1]);

    msg_schedule_remove_t remove_cmd;
    remove_cmd.schedule_id = schedule_id;

    msg_t msg;
    msg.msg_id = MSG_0401;
    msg.data_size = sizeof(msg_schedule_remove_t);
    msg.data_bytes = (u8*)&remove_cmd;

    cli_print("Removing schedule ID: %d", schedule_id);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_schedule_list(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0402;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_schedule_clear(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;

    msg_t msg;
    msg.msg_id = MSG_0403;
    msg.data_size = 0;
    msg.data_bytes = NULL;

    cli_print("Clearing all schedules...");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

static int prv_cmd_schedule_enable(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 2)
    {
        cli_print("Usage: schedule_enable <0|1>");
        cli_print("  0 = disable scheduling");
        cli_print("  1 = enable scheduling");
        return CLI_FAIL_STATUS;
    }

    int enabled = atoi(argv[1]);

    msg_schedule_enable_t enable_cmd;
    enable_cmd.enabled = (enabled != 0);

    msg_t msg;
    msg.msg_id = MSG_0404;
    msg.data_size = sizeof(msg_schedule_enable_t);
    msg.data_bytes = (u8*)&enable_cmd;

    cli_print("Scheduling %s", enabled ? "enabled" : "disabled");
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}

// ============================
// = Logging Commands
// ============================

static int prv_cmd_log(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc != 3)
    {
        cli_print("Usage: log <on|off> <module_name>");
        cli_print("Available modules:");
        cli_print("  appcontrol  - Application Control module");
        cli_print("  mp3player   - MP3 Player module");
        cli_print("  timesync    - Time Sync module");
        cli_print("  wifimanager - WiFi Manager module");
        cli_print("  all         - All modules");
        return CLI_FAIL_STATUS;
    }

    bool enable = false;
    if (strcmp(argv[1], "on") == 0)
    {
        enable = true;
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        enable = false;
    }
    else
    {
        cli_print("Invalid parameter. Use 'on' or 'off'");
        return CLI_FAIL_STATUS;
    }

    msg_set_logging_t log_cmd;
    log_cmd.enabled = enable;

    // Set module ID based on name
    if (strcmp(argv[2], "appcontrol") == 0)
    {
        log_cmd.module_id = MODULE_APPCONTROL;
    }
    else if (strcmp(argv[2], "mp3player") == 0)
    {
        log_cmd.module_id = MODULE_MP3PLAYER;
    }
    else if (strcmp(argv[2], "timesync") == 0)
    {
        log_cmd.module_id = MODULE_TIMESYNC;
    }
    else if (strcmp(argv[2], "wifimanager") == 0)
    {
        log_cmd.module_id = MODULE_WIFIMANAGER;
    }
    else if (strcmp(argv[2], "all") == 0)
    {
        log_cmd.module_id = MODULE_ALL;
    }
    else
    {
        cli_print("Unknown module: %s", argv[2]);
        return CLI_FAIL_STATUS;
    }

    // Copy module name
    strncpy(log_cmd.module_name, argv[2], MODULE_NAME_MAX_LENGTH - 1);
    log_cmd.module_name[MODULE_NAME_MAX_LENGTH - 1] = '\0';

    msg_t msg;
    msg.msg_id = MSG_0003;
    msg.data_size = sizeof(msg_set_logging_t);
    msg.data_bytes = (u8*)&log_cmd;

    cli_print("Logging %s for module: %s", enable ? "enabled" : "disabled", argv[2]);
    messagebroker_publish(&msg);

    return CLI_OK_STATUS;
}
