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
 * @file WiFiManager.cpp
 * @brief WiFi credential management and connection handling
 */

#include "WiFiManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "MessageBroker.h"
#include "MessageDefinitions.h"
#include "custom_assert.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ###########################################################################
// # Private defines
// ###########################################################################
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_CHECK_INTERVAL_MS  5000
#define PREFERENCES_NAMESPACE   "wifi"
#define PREF_KEY_SSID           "ssid"
#define PREF_KEY_PASSWORD       "password"

// ###########################################################################
// # Private function declarations
// ###########################################################################
static void prv_wifimanager_task(void* parameter);
static void prv_message_broker_callback(const msg_t* const message);
static void prv_connect_to_wifi(void);
static void prv_save_credentials(const char* ssid, const char* password);
static bool prv_load_credentials(char* ssid, char* password);
static void prv_publish_connection_status(void);

// ###########################################################################
// # Private Variables
// ###########################################################################
static bool is_initialized = false;
static bool is_connected = false;
static bool logging_enabled = false; // Logging initially disabled
static TaskHandle_t wifimanager_task_handle = NULL;
static Preferences preferences;

static char current_ssid[WIFI_SSID_MAX_LENGTH] = {0};
static char current_password[WIFI_PASSWORD_MAX_LENGTH] = {0};
static bool has_credentials = false;

// ###########################################################################
// # Public function implementations
// ###########################################################################

void wifimanager_init(void)
{
    ASSERT(!is_initialized);

    // Initialize preferences
    preferences.begin(PREFERENCES_NAMESPACE, false);

    // Load stored credentials
    has_credentials = prv_load_credentials(current_ssid, current_password);

    // Subscribe to WiFi messages
    messagebroker_subscribe(MSG_0003, prv_message_broker_callback); // Logging control
    messagebroker_subscribe(MSG_0200, prv_message_broker_callback);
    messagebroker_subscribe(MSG_0201, prv_message_broker_callback);

    is_initialized = true;

    if (has_credentials)
    {
        Serial.println("[WiFiManager] Loaded credentials from storage");
    }
    else
    {
        Serial.println("[WiFiManager] No credentials stored");
    }
}

void wifimanager_start_task(void)
{
    ASSERT(is_initialized);

    if (wifimanager_task_handle == NULL)
    {
        xTaskCreate(prv_wifimanager_task, "WiFiManagerTask", 4096, NULL, 1, &wifimanager_task_handle);
    }
}

bool wifimanager_is_connected(void) { return is_connected; }

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_wifimanager_task(void* parameter)
{
    (void)parameter;

    while (1)
    {
        // Check current WiFi status
        wl_status_t status = WiFi.status();

        if (status == WL_CONNECTED)
        {
            if (!is_connected)
            {
                is_connected = true;
                Serial.println("[WiFiManager] Connected to WiFi");
                Serial.printf("[WiFiManager] IP Address: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("[WiFiManager] RSSI: %d dBm\n", WiFi.RSSI());
                prv_publish_connection_status();
            }
        }
        else
        {
            if (is_connected)
            {
                is_connected = false;
                Serial.println("[WiFiManager] Disconnected from WiFi");
                prv_publish_connection_status();
            }

            // Try to reconnect if we have credentials
            if (has_credentials)
            {
                Serial.println("[WiFiManager] Attempting to reconnect...");
                prv_connect_to_wifi();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_CHECK_INTERVAL_MS));
    }
}

static void prv_connect_to_wifi(void)
{
    if (!has_credentials)
    {
        if (logging_enabled)
        {
            Serial.println("[WiFiManager] No credentials available");
        }
        return;
    }

    if (logging_enabled)
    {
        Serial.printf("[WiFiManager] Connecting to SSID: %s\n", current_ssid);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(current_ssid, current_password);

    // Publish connecting status
    msg_wifi_connection_status_t status_msg;
    status_msg.status = WIFI_STATUS_CONNECTING;
    strncpy(status_msg.ssid, current_ssid, WIFI_SSID_MAX_LENGTH - 1);
    status_msg.ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';
    status_msg.rssi = 0;

    msg_t msg;
    msg.msg_id = MSG_0203;
    msg.data_size = sizeof(msg_wifi_connection_status_t);
    msg.data_bytes = (u8*)&status_msg;
    messagebroker_publish(&msg);

    // Wait for connection
    unsigned long start_time = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_time) < WIFI_CONNECT_TIMEOUT_MS)
    {
        delay(500);
        if (logging_enabled)
        {
            Serial.print(".");
        }
    }
    if (logging_enabled)
    {
        Serial.println();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        is_connected = true;
        if (logging_enabled)
        {
            Serial.println("[WiFiManager] Successfully connected");
        }
        prv_publish_connection_status();
    }
    else
    {
        is_connected = false;
        if (logging_enabled)
        {
            Serial.println("[WiFiManager] Connection failed");
        }

        status_msg.status = WIFI_STATUS_FAILED;
        status_msg.rssi = 0;
        messagebroker_publish(&msg);
    }
}

static void prv_save_credentials(const char* ssid, const char* password)
{
    preferences.putString(PREF_KEY_SSID, ssid);
    preferences.putString(PREF_KEY_PASSWORD, password);

    strncpy(current_ssid, ssid, WIFI_SSID_MAX_LENGTH - 1);
    current_ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';

    strncpy(current_password, password, WIFI_PASSWORD_MAX_LENGTH - 1);
    current_password[WIFI_PASSWORD_MAX_LENGTH - 1] = '\0';

    has_credentials = true;

    if (logging_enabled)
    {
        Serial.println("[WiFiManager] Credentials saved to storage");
    }
}

static bool prv_load_credentials(char* ssid, char* password)
{
    String stored_ssid = preferences.getString(PREF_KEY_SSID, "");
    String stored_password = preferences.getString(PREF_KEY_PASSWORD, "");

    if (stored_ssid.length() == 0)
    {
        return false;
    }

    strncpy(ssid, stored_ssid.c_str(), WIFI_SSID_MAX_LENGTH - 1);
    ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';

    strncpy(password, stored_password.c_str(), WIFI_PASSWORD_MAX_LENGTH - 1);
    password[WIFI_PASSWORD_MAX_LENGTH - 1] = '\0';

    return true;
}

static void prv_publish_connection_status(void)
{
    msg_wifi_connection_status_t status_msg;

    if (is_connected)
    {
        status_msg.status = WIFI_STATUS_CONNECTED;
        strncpy(status_msg.ssid, WiFi.SSID().c_str(), WIFI_SSID_MAX_LENGTH - 1);
        status_msg.ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';
        status_msg.rssi = WiFi.RSSI();
    }
    else
    {
        status_msg.status = WIFI_STATUS_DISCONNECTED;
        status_msg.ssid[0] = '\0';
        status_msg.rssi = 0;
    }

    msg_t msg;
    msg.msg_id = MSG_0203;
    msg.data_size = sizeof(msg_wifi_connection_status_t);
    msg.data_bytes = (u8*)&status_msg;

    messagebroker_publish(&msg);
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
            if (cmd->module_id == MODULE_WIFIMANAGER || cmd->module_id == MODULE_ALL ||
                strncmp(cmd->module_name, "wifimanager", MODULE_NAME_MAX_LENGTH) == 0)
            {
                logging_enabled = cmd->enabled;
                if (logging_enabled)
                {
                    Serial.println("[WiFiManager] Logging enabled");
                }
            }
            break;
        }

        case MSG_0200:
        {
            ASSERT(message->data_size == sizeof(msg_wifi_set_credentials_t));
            msg_wifi_set_credentials_t* creds = (msg_wifi_set_credentials_t*)message->data_bytes;

            // Save credentials
            prv_save_credentials(creds->ssid, creds->password);

            // Disconnect current connection if any
            if (WiFi.status() == WL_CONNECTED)
            {
                WiFi.disconnect();
                is_connected = false;
            }

            // Connect with new credentials
            prv_connect_to_wifi();
            break;
        }

        case MSG_0201:
        {
            msg_wifi_credentials_response_t response;
            response.has_credentials = has_credentials;

            if (has_credentials)
            {
                strncpy(response.ssid, current_ssid, WIFI_SSID_MAX_LENGTH - 1);
                response.ssid[WIFI_SSID_MAX_LENGTH - 1] = '\0';

                strncpy(response.password, current_password, WIFI_PASSWORD_MAX_LENGTH - 1);
                response.password[WIFI_PASSWORD_MAX_LENGTH - 1] = '\0';
            }
            else
            {
                response.ssid[0] = '\0';
                response.password[0] = '\0';
            }

            msg_t response_msg;
            response_msg.msg_id = MSG_0202;
            response_msg.data_size = sizeof(msg_wifi_credentials_response_t);
            response_msg.data_bytes = (u8*)&response;

            messagebroker_publish(&response_msg);
            break;
        }

        default: break;
    }
}
