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
static bool is_initialized = false;
static bool time_synchronized = false;
static TaskHandle_t timesync_task_handle = NULL;

// ###########################################################################
// # Public function implementations
// ###########################################################################

void timesync_init(void)
{
    ASSERT(!is_initialized);

    // Subscribe to time request messages
    messagebroker_subscribe(MSG_0100, prv_message_broker_callback);

    is_initialized = true;
}

void timesync_start_task(void)
{
    ASSERT(is_initialized);

    if (timesync_task_handle == NULL)
    {
        xTaskCreate(prv_timesync_task, "TimeSyncTask", 4096, NULL, 1, &timesync_task_handle);
    }
}

bool timesync_is_synchronized(void) { return time_synchronized; }

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_timesync_task(void* parameter)
{
    (void)parameter;

    TickType_t last_sync_time = 0;

    while (1)
    {
        // Check if WiFi is connected
        if (WiFi.status() == WL_CONNECTED)
        {
            TickType_t current_time = xTaskGetTickCount();

            // Sync time if not synchronized yet or if sync interval has passed
            if (!time_synchronized || (current_time - last_sync_time) >= pdMS_TO_TICKS(SYNC_INTERVAL_MS))
            {
                prv_sync_time_from_ntp();
                last_sync_time = current_time;
            }
        }
        else
        {
            time_synchronized = false;
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
            time_synchronized = true;
            Serial.println("[TimeSync] Time synchronized with NTP server");
            Serial.printf("[TimeSync] Current time: %s", asctime(&timeinfo));
            return;
        }
        retry++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Serial.println("[TimeSync] Failed to synchronize time with NTP server");
    time_synchronized = false;
}

static void prv_message_broker_callback(const msg_t* const message)
{
    ASSERT(message != NULL);

    switch (message->msg_id)
    {
        case MSG_0100:
        {
            msg_time_get_response_t response;
            response.time_valid = time_synchronized;

            if (time_synchronized)
            {
                time(&response.timestamp);
                getLocalTime(&response.timeinfo);
            }
            else
            {
                response.timestamp = 0;
                memset(&response.timeinfo, 0, sizeof(struct tm));
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
