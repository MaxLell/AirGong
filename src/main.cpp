#include <Arduino.h>
#include "BlinkLed.h"
#include "Console.h"
#include "MessageBroker.h"
#include "TimeSync.h"
#include "WiFiManager.h"
#include "custom_assert.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ###########################################################################
// # Private function declarations
// ###########################################################################
void console_task(void* parameter);

static void prv_assert_failed(const char* file, uint32_t line, const char* expr);
static void msg_broker_callback(const msg_t* const message);

// ###########################################################################
// # Task handles
// ###########################################################################
TaskHandle_t console_task_handle = NULL;

// ###########################################################################
// # Private Data
// ###########################################################################

// ###########################################################################
// # Setup and Loop
// ###########################################################################

#define LED_PIN LED_BUILTIN

void setup()
{
    // Initialize custom assert
    custom_assert_init(prv_assert_failed);

    messagebroker_init();

    // Initialize WiFi Manager
    wifimanager_init();
    wifimanager_start_task();

    // Initialize Time Sync
    timesync_init();
    timesync_start_task();

    // Create console task
    xTaskCreate(console_task,        // Task function
                "ConsoleTask",       // Task name
                4096,                // Stack size (words)
                NULL,                // Task parameters
                1,                   // Task priority (0 = lowest, configMAX_PRIORITIES - 1 = highest)
                &console_task_handle // Task handle
    );

    // Initialize Blink LED
    blinkled_init(LED_PIN);
}

void loop() {}

// ###########################################################################
// # Task implementations
// ###########################################################################

void console_task(void* parameter)
{
    (void)parameter; // Unused parameter

    // Initialize console
    console_init();

    // Task main loop
    while (1)
    {
        // Run the console processing
        console_run();

        delay(5);
    }
}

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_assert_failed(const char* file, uint32_t line, const char* expr)
{
    Serial.printf("[ASSERT FAILED]: %s:%u - %s\n", file, line, expr);
    // In embedded systems, we might want to reset instead of infinite loop

    // Stop all tasks
    vTaskSuspend(console_task_handle);

    while (1)
    {
        blinkled_toggle();
        delay(700); // Keep watchdog happy if enabled
    }
}

static void msg_broker_callback(const msg_t* const message)
{
    ASSERT(message != NULL);

    switch (message->msg_id)
    {

        default:
            // Unknown message ID
            ASSERT(false);
            break;
    }
}