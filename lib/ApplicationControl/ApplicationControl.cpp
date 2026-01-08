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
 * @file ApplicationControl.cpp
 * @brief Application Control module implementation
 *
 * Manages scheduled song playback based on configured time schedules.
 */

#include "ApplicationControl.h"
#include "MessageBroker.h"
#include "MessageDefinitions.h"
#include "custom_assert.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

// ###########################################################################
// # Defines
// ###########################################################################
#define MAX_SCHEDULES           20
#define NVS_NAMESPACE           "appcontrol"
#define NVS_KEY_SCHEDULE_COUNT  "sched_cnt"
#define NVS_KEY_SCHEDULE_PREFIX "sched_"

// ###########################################################################
// # Type Definitions
// ###########################################################################
typedef struct
{
    bool active;         // Whether this schedule entry is active
    u8 hour;             // Hour (0-23)
    u8 minute;           // Minute (0-59)
    u16 song_index;      // Song index to play
    bool triggered;      // Flag to avoid re-triggering in the same minute
    u8 last_checked_min; // Last checked minute to detect minute changes
} schedule_entry_t;

// ###########################################################################
// # Private function declarations
// ###########################################################################
static void prv_message_handler(const msg_t* const message);
static void prv_check_schedules(const struct tm* timeinfo);
static void prv_play_song(u16 song_index);
static void prv_save_schedules_to_flash(void);
static void prv_load_schedules_from_flash(void);

// ###########################################################################
// # Private Variables
// ###########################################################################
static bool is_initialized = false;
static bool scheduling_enabled = true;
static bool g_logging_is_active = false; // Logging initially disabled
static schedule_entry_t schedules[MAX_SCHEDULES];
static struct tm current_time;
static bool time_valid = false;
static Preferences preferences;

// ###########################################################################
// # Public function implementations
// ###########################################################################

void appcontrol_init(void)
{
    ASSERT(!is_initialized);

    // Initialize schedule array
    for (int i = 0; i < MAX_SCHEDULES; i++)
    {
        schedules[i].active = false;
        schedules[i].hour = 0;
        schedules[i].minute = 0;
        schedules[i].song_index = 0;
        schedules[i].triggered = false;
        schedules[i].last_checked_min = 255; // Invalid value to force initial check
    }

    // Load schedules from flash
    prv_load_schedules_from_flash();

    // Subscribe to messages
    messagebroker_subscribe(MSG_0003, prv_message_handler); // Logging control
    messagebroker_subscribe(MSG_0101, prv_message_handler); // Time response
    messagebroker_subscribe(MSG_0400, prv_message_handler); // Add schedule
    messagebroker_subscribe(MSG_0401, prv_message_handler); // Remove schedule
    messagebroker_subscribe(MSG_0402, prv_message_handler); // List schedules
    messagebroker_subscribe(MSG_0403, prv_message_handler); // Clear schedules
    messagebroker_subscribe(MSG_0404, prv_message_handler); // Enable/disable scheduling

    is_initialized = true;
}

void appcontrol_run(void)
{
    ASSERT(is_initialized);

    if (!scheduling_enabled)
    {
        return;
    }

    // Request current time every cycle (even if not valid yet)
    msg_time_get_request_t request;
    msg_t msg;
    msg.msg_id = MSG_0100;
    msg.data_size = sizeof(msg_time_get_request_t);
    msg.data_bytes = (u8*)&request;
    messagebroker_publish(&msg);
}

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_message_handler(const msg_t* const message)
{
    switch (message->msg_id)
    {
        case MSG_0003: // Set logging
        {
            msg_set_logging_t* cmd = (msg_set_logging_t*)message->data_bytes;

            // Check if this message is for us
            if (cmd->module_id == MODULE_APPCONTROL || cmd->module_id == MODULE_ALL
                || strncmp(cmd->module_name, "appcontrol", MODULE_NAME_MAX_LENGTH) == 0)
            {
                g_logging_is_active = cmd->enabled;
                if (g_logging_is_active)
                {
                    Serial.println("[AppControl] Logging enabled");
                }
            }
            break;
        }

        case MSG_0101: // Time response
        {
            msg_time_get_response_t* response = (msg_time_get_response_t*)message->data_bytes;
            time_valid = response->time_valid;
            if (time_valid)
            {
                current_time = response->timeinfo;
                prv_check_schedules(&current_time);
            }
            break;
        }

        case MSG_0400: // Add schedule
        {
            msg_schedule_add_t* cmd = (msg_schedule_add_t*)message->data_bytes;

            // Find free slot
            int free_slot = -1;
            for (int i = 0; i < MAX_SCHEDULES; i++)
            {
                if (!schedules[i].active)
                {
                    free_slot = i;
                    break;
                }
            }

            msg_schedule_response_t response;
            if (free_slot >= 0)
            {
                schedules[free_slot].active = true;
                schedules[free_slot].hour = cmd->hour;
                schedules[free_slot].minute = cmd->minute;
                schedules[free_slot].song_index = cmd->song_index;
                schedules[free_slot].triggered = false;
                schedules[free_slot].last_checked_min = 255;

                // Save to flash
                prv_save_schedules_to_flash();

                response.success = true;
                response.schedule_id = free_slot;
            }
            else
            {
                response.success = false;
                response.schedule_id = -1;
            }

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0405;
            resp_msg.data_size = sizeof(msg_schedule_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0401: // Remove schedule
        {
            msg_schedule_remove_t* cmd = (msg_schedule_remove_t*)message->data_bytes;

            msg_schedule_response_t response;
            if (cmd->schedule_id >= 0 && cmd->schedule_id < MAX_SCHEDULES)
            {
                schedules[cmd->schedule_id].active = false;

                // Save to flash
                prv_save_schedules_to_flash();

                response.success = true;
                response.schedule_id = cmd->schedule_id;
            }
            else
            {
                response.success = false;
                response.schedule_id = -1;
            }

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0405;
            resp_msg.data_size = sizeof(msg_schedule_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0402: // List schedules
        {
            msg_schedule_list_t list;
            list.count = 0;

            for (int i = 0; i < MAX_SCHEDULES && list.count < 20; i++)
            {
                if (schedules[i].active)
                {
                    list.schedules[list.count].schedule_id = i;
                    list.schedules[list.count].hour = schedules[i].hour;
                    list.schedules[list.count].minute = schedules[i].minute;
                    list.schedules[list.count].song_index = schedules[i].song_index;
                    list.count++;
                }
            }

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0406;
            resp_msg.data_size = sizeof(msg_schedule_list_t);
            resp_msg.data_bytes = (u8*)&list;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0403: // Clear all schedules
        {
            for (int i = 0; i < MAX_SCHEDULES; i++)
            {
                schedules[i].active = false;
            }

            // Save to flash
            prv_save_schedules_to_flash();

            msg_schedule_response_t response;
            response.success = true;
            response.schedule_id = -1;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0405;
            resp_msg.data_size = sizeof(msg_schedule_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0404: // Enable/disable scheduling
        {
            msg_schedule_enable_t* cmd = (msg_schedule_enable_t*)message->data_bytes;
            scheduling_enabled = cmd->enabled;

            msg_schedule_response_t response;
            response.success = true;
            response.schedule_id = -1;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0405;
            resp_msg.data_size = sizeof(msg_schedule_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        default: break;
    }
}

static void prv_check_schedules(const struct tm* timeinfo)
{
    u8 current_hour = timeinfo->tm_hour;
    u8 current_min = timeinfo->tm_min;

    for (int i = 0; i < MAX_SCHEDULES; i++)
    {
        if (!schedules[i].active)
        {
            continue;
        }

        // Check if minute changed since last check
        if (schedules[i].last_checked_min != current_min)
        {
            schedules[i].last_checked_min = current_min;
            schedules[i].triggered = false; // Reset trigger flag on minute change
        }

        // Check if time matches and hasn't been triggered yet this minute
        if (schedules[i].hour == current_hour && schedules[i].minute == current_min && !schedules[i].triggered)
        {
            if (g_logging_is_active)
            {
                Serial.printf("[AppControl] Triggering schedule %d: Playing song %d at %02d:%02d\n", i,
                              schedules[i].song_index, current_hour, current_min);
            }

            // Trigger song playback
            prv_play_song(schedules[i].song_index);
            schedules[i].triggered = true;
        }
    }
}

static void prv_play_song(u16 song_index)
{
    Serial.println("ApplicationControl: Playing scheduled song index " + String(song_index));

    msg_mp3_play_song_t play_cmd;
    play_cmd.song_index = song_index;

    msg_t msg;
    msg.msg_id = MSG_0302;
    msg.data_size = sizeof(msg_mp3_play_song_t);
    msg.data_bytes = (u8*)&play_cmd;

    messagebroker_publish(&msg);
}

static void prv_save_schedules_to_flash(void)
{
    preferences.begin(NVS_NAMESPACE, false); // Open in read-write mode

    // Count active schedules
    int active_count = 0;
    for (int i = 0; i < MAX_SCHEDULES; i++)
    {
        if (schedules[i].active)
        {
            active_count++;
        }
    }

    // Save count
    preferences.putInt(NVS_KEY_SCHEDULE_COUNT, active_count);

    // Save each active schedule
    int save_index = 0;
    for (int i = 0; i < MAX_SCHEDULES; i++)
    {
        if (schedules[i].active)
        {
            char key[16];
            // Save schedule data as a struct
            snprintf(key, sizeof(key), "%s%d", NVS_KEY_SCHEDULE_PREFIX, save_index);

            // Create a packed structure for storage
            struct
            {
                u8 hour;
                u8 minute;
                u16 song_index;
                int original_id; // Store original array position
            } storage_data;

            storage_data.hour = schedules[i].hour;
            storage_data.minute = schedules[i].minute;
            storage_data.song_index = schedules[i].song_index;
            storage_data.original_id = i;

            preferences.putBytes(key, &storage_data, sizeof(storage_data));
            save_index++;
        }
    }

    preferences.end();
}

static void prv_load_schedules_from_flash(void)
{
    preferences.begin(NVS_NAMESPACE, true); // Open in read-only mode

    // Get count of saved schedules
    int saved_count = preferences.getInt(NVS_KEY_SCHEDULE_COUNT, 0);

    if (saved_count > 0)
    {
        // Load each schedule
        for (int i = 0; i < saved_count && i < MAX_SCHEDULES; i++)
        {
            char key[16];
            snprintf(key, sizeof(key), "%s%d", NVS_KEY_SCHEDULE_PREFIX, i);

            // Read stored data
            struct
            {
                u8 hour;
                u8 minute;
                u16 song_index;
                int original_id;
            } storage_data;

            size_t len = preferences.getBytes(key, &storage_data, sizeof(storage_data));

            if (len == sizeof(storage_data))
            {
                // Try to restore to original position, or find next free slot
                int target_slot = storage_data.original_id;
                if (target_slot >= MAX_SCHEDULES || schedules[target_slot].active)
                {
                    // Find a free slot
                    target_slot = -1;
                    for (int j = 0; j < MAX_SCHEDULES; j++)
                    {
                        if (!schedules[j].active)
                        {
                            target_slot = j;
                            break;
                        }
                    }
                }

                if (target_slot >= 0)
                {
                    schedules[target_slot].active = true;
                    schedules[target_slot].hour = storage_data.hour;
                    schedules[target_slot].minute = storage_data.minute;
                    schedules[target_slot].song_index = storage_data.song_index;
                    schedules[target_slot].triggered = false;
                    schedules[target_slot].last_checked_min = 255;
                }
            }
        }
    }

    preferences.end();
}
