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

    // Logging Commands

};

// ###########################################################################
// # Public function implementations
// ###########################################################################

void console_init(void)
{
    ASSERT(!is_initialized);

    // Initialize Serial communication
    Serial.begin(115200);

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

    // Subscribe to a test message
    messagebroker_subscribe(MSG_0001, prv_msg_broker_callback);

    cli_print("Subscribed to MSG_0001 \n... \nNow publishing a test message. \n...");
    // Publish a test message
    msg_t test_msg;
    test_msg.msg_id = MSG_0001;
    const char* test_data = "The elephant has been tickled!";
    test_msg.data_size = strlen(test_data) + 1; // Including null terminator
    test_msg.data_bytes = (u8*)test_data;

    messagebroker_publish(&test_msg);

    return CLI_OK_STATUS;
}
