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
 * @file TimeSync.cpp
 * @brief Time synchronization module using NTP over WiFi
 */

#include "TimeSync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>
#include "MessageBroker.h"
#include "MessageDefinitions.h"
#include "custom_assert.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ###########################################################################
// # Private defines
// ###########################################################################
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      3600    // GMT+1 for Central European Time
#define DAYLIGHT_OFFSET_SEC 3600    // Daylight saving time offset
#define SYNC_INTERVAL_MS    3600000 // Sync every hour

// ###########################################################################
// # Private function declarations
// ###########################################################################
static void prv_timesync_task(void* parameter);
static void prv_message_broker_callback(const msg_t* const message);
static void prv_sync_time_from_ntp(void);

// ###########################################################################
// # Private Variables
// ###########################################################################
static bool g_is_initialized = false;
static bool g_time_is_synchronized = false;
static bool g_ntp_sync_was_successful = false; // Tracks if NTP was ever successful
static bool g_logging_is_active = false;       // Logging initially disabled
static TaskHandle_t timesync_task_handle = NULL;
static time_t g_last_ntp_sync_time = 0; // Track when last NTP sync occurred

// ###########################################################################
// # Public function implementations
// ###########################################################################

void timesync_init(void)
{
    ASSERT(!g_is_initialized);

    // Subscribe to time request messages
    messagebroker_subscribe(MSG_0003, prv_message_broker_callback); // Logging control
    messagebroker_subscribe(MSG_0100, prv_message_broker_callback); // Time request

    g_is_initialized = true;
}

void timesync_start_task(void)
{
    ASSERT(g_is_initialized);

    if (timesync_task_handle == NULL)
    {
        xTaskCreate(prv_timesync_task, "TimeSyncTask", 4096, NULL, 1, &timesync_task_handle);
    }
}

bool timesync_is_synchronized(void) { return g_time_is_synchronized; }

bool timesync_get_time(struct tm* timeinfo)
{
    ASSERT(timeinfo != NULL);

    if (timeinfo == NULL)
    {
        return false;
    }

    // Try to get local time (works with both NTP and RTC)
    if (getLocalTime(timeinfo))
    {
        return true;
    }

    return false;
}

time_t timesync_get_timestamp(void)
{
    time_t now;
    time(&now);

    // Return timestamp only if time is valid (not close to epoch)
    if (now > 1000000000) // Timestamp after year 2001
    {
        return now;
    }

    return 0;
}

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_timesync_task(void* parameter)
{
    (void)parameter;

    TickType_t last_sync_time = 0;
    bool initial_sync_done = false;

    while (1)
    {
        // Check if WiFi is connected
        if (WiFi.status() == WL_CONNECTED)
        {
            TickType_t current_time = xTaskGetTickCount();

            // Sync time if not synchronized yet or if sync interval has passed (1 hour)
            if (!initial_sync_done || (current_time - last_sync_time) >= pdMS_TO_TICKS(SYNC_INTERVAL_MS))
            {
                prv_sync_time_from_ntp();
                last_sync_time = current_time;
                initial_sync_done = true;

                if (g_ntp_sync_was_successful)
                {
                    // Update RTC with NTP time
                    time_t now;
                    time(&now);
                    g_last_ntp_sync_time = now;

                    if (g_logging_is_active)
                    {
                        Serial.println("[TimeSync] RTC updated with NTP time");
                    }
                }
            }

            // Time is considered synchronized if we have valid time (from NTP or RTC)
            g_time_is_synchronized = (timesync_get_timestamp() > 0);
        }
        else
        {
            // WiFi not connected - check if we have valid RTC time
            time_t current_time = timesync_get_timestamp();

            if (current_time > 0)
            {
                // RTC has valid time, mark as synchronized
                g_time_is_synchronized = true;

                if (g_logging_is_active && !initial_sync_done)
                {
                    Serial.println("[TimeSync] WiFi not available, using RTC time");
                    initial_sync_done = true; // Prevent repeated messages
                }
            }
            else
            {
                g_time_is_synchronized = false;

                if (g_logging_is_active)
                {
                    Serial.println("[TimeSync] No valid time source available (no WiFi, no RTC)");
                }
            }
        }

        // Wait for 10 seconds before checking again
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void prv_sync_time_from_ntp(void)
{
    // Configure time with NTP server
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    // Wait for time to be set (max 10 seconds)
    int retry = 0;
    const int retry_count = 10;
    struct tm timeinfo;

    while (retry < retry_count)
    {
        if (getLocalTime(&timeinfo))
        {
            g_ntp_sync_was_successful = true;
            g_time_is_synchronized = true;

            // Store timestamp of successful sync
            time(&g_last_ntp_sync_time);

            if (g_logging_is_active)
            {
                Serial.println("[TimeSync] Time synchronized with NTP server");
                Serial.printf("[TimeSync] Current time: %s", asctime(&timeinfo));
                Serial.printf("[TimeSync] Next sync in 1 hour\n");
            }
            return;
        }
        retry++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (g_logging_is_active)
    {
        Serial.println("[TimeSync] Failed to synchronize time with NTP server");

        // Check if we can fall back to RTC
        time_t rtc_time = timesync_get_timestamp();
        if (rtc_time > 0)
        {
            Serial.println("[TimeSync] Falling back to RTC time");
        }
    }
}

static void prv_message_broker_callback(const msg_t* const message)
{
    ASSERT(message != NULL);

    switch (message->msg_id)
    {
        case MSG_0003: // Set logging
        {
            msg_set_logging_t* cmd = (msg_set_logging_t*)message->data_bytes;

            // Check if this message is for us
            if (cmd->module_id == MODULE_TIMESYNC || cmd->module_id == MODULE_ALL
                || strncmp(cmd->module_name, "timesync", MODULE_NAME_MAX_LENGTH) == 0)
            {
                g_logging_is_active = cmd->enabled;
                if (g_logging_is_active)
                {
                    Serial.println("[TimeSync] Logging enabled");
                }
            }
            break;
        }

        case MSG_0100:
        {
            msg_time_get_response_t response;

            // Try to get time from either NTP or RTC
            response.timestamp = timesync_get_timestamp();
            response.time_valid = timesync_get_time(&response.timeinfo);

            if (response.time_valid)
            {
                if (g_logging_is_active)
                {
                    Serial.println("[TimeSync] Time request received");
                    Serial.printf("[TimeSync] Current time: %s", asctime(&response.timeinfo));
                    Serial.printf("[TimeSync] Unix timestamp: %ld\n", (long)response.timestamp);

                    // Indicate source
                    if (WiFi.status() == WL_CONNECTED && g_ntp_sync_was_successful)
                    {
                        Serial.println("[TimeSync] Time source: NTP");
                    }
                    else
                    {
                        Serial.println("[TimeSync] Time source: RTC");
                    }
                }
            }
            else
            {
                response.timestamp = 0;
                memset(&response.timeinfo, 0, sizeof(struct tm));

                if (g_logging_is_active)
                {
                    Serial.println("[TimeSync] Time request received but no valid time source available");
                }
            }

            msg_t response_msg;
            response_msg.msg_id = MSG_0101;
            response_msg.data_size = sizeof(msg_time_get_response_t);
            response_msg.data_bytes = (u8*)&response;

            messagebroker_publish(&response_msg);
            break;
        }

        default: break;
    }
}
