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
 * @file MP3Player.cpp
 * @brief MP3 Player module implementation for WT2605C
 *
 * This module implements the interface to control the WT2605C MP3 player.
 * For ESP32C6, Serial0 is used for communication with the MP3 player.
 */

#include "MP3Player.h"
#include "MessageBroker.h"
#include "MessageDefinitions.h"
#include "custom_assert.h"

#include <Arduino.h>
#include "WT2605C_Player.h"

// ###########################################################################
// # Private function declarations
// ###########################################################################
static void prv_mp3_message_handler(const msg_t* const message);

// ###########################################################################
// # Private Variables
// ###########################################################################

static bool is_initialized = false;

// MP3 Player instance - für ESP32C6 verwenden wir HardwareSerial
static WT2605C<HardwareSerial> mp3_player;

// ###########################################################################
// # Public function implementations
// ###########################################################################

void mp3player_init(void)
{
    ASSERT(!is_initialized);

    // Initialize Serial0 for communication with WT2605C (115200 baud)
    Serial0.begin(115200);

    // Initialize the MP3 player
    mp3_player.init(Serial0);

    // Subscribe to MP3 control messages
    messagebroker_subscribe(MSG_0300, prv_mp3_message_handler); // Set volume
    messagebroker_subscribe(MSG_0301, prv_mp3_message_handler); // Set play mode
    messagebroker_subscribe(MSG_0302, prv_mp3_message_handler); // Play song by index
    messagebroker_subscribe(MSG_0303, prv_mp3_message_handler); // Volume up
    messagebroker_subscribe(MSG_0304, prv_mp3_message_handler); // Volume down
    messagebroker_subscribe(MSG_0305, prv_mp3_message_handler); // Next song
    messagebroker_subscribe(MSG_0306, prv_mp3_message_handler); // Previous song
    messagebroker_subscribe(MSG_0307, prv_mp3_message_handler); // Pause or play

    is_initialized = true;
}

void mp3player_run(void)
{
    ASSERT(is_initialized);

    // Periodic tasks if needed
    // Currently no periodic tasks required
}

// ###########################################################################
// # Private function implementations
// ###########################################################################

static void prv_mp3_message_handler(const msg_t* const message)
{
    int result = 0;

    switch (message->msg_id)
    {
        case MSG_0300: // Set volume
        {
            msg_mp3_set_volume_t* cmd = (msg_mp3_set_volume_t*)message->data_bytes;
            result = mp3_player.volume(cmd->volume);

            // Send response
            msg_mp3_command_response_t response;
            response.success = (result == 0);
            response.error_code = result;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0301: // Set play mode
        {
            msg_mp3_set_playmode_t* cmd = (msg_mp3_set_playmode_t*)message->data_bytes;

            switch (cmd->mode)
            {
                case MP3_MODE_LOOP: result = mp3_player.playMode(WT2605C_CYCLE); break;
                case MP3_MODE_SINGLE_LOOP: result = mp3_player.playMode(WT2605C_SINGLE_CYCLE); break;
                case MP3_MODE_FOLDER_LOOP: result = mp3_player.playMode(WT2605C_DIR_CYCLE); break;
                case MP3_MODE_RANDOM: result = mp3_player.playMode(WT2605C_RANDOM); break;
                case MP3_MODE_SINGLE_SHOT: result = mp3_player.playMode(WT2605C_SINGLE_SHOT); break;
                default:
                    result = -1; // Invalid mode
                    break;
            }

            // Send response
            msg_mp3_command_response_t response;
            response.success = (result == 0);
            response.error_code = result;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0302: // Play song by index
        {
            msg_mp3_play_song_t* cmd = (msg_mp3_play_song_t*)message->data_bytes;
            mp3_player.playSDRootSong(cmd->song_index);

            // Send response
            msg_mp3_command_response_t response;
            response.success = true;
            response.error_code = 0;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0303: // Volume up
        {
            result = mp3_player.volumeUp();

            // Send response
            msg_mp3_command_response_t response;
            response.success = (result == 0);
            response.error_code = result;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0304: // Volume down
        {
            result = mp3_player.volumeDown();

            // Send response
            msg_mp3_command_response_t response;
            response.success = (result == 0);
            response.error_code = result;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0305: // Next song
        {
            mp3_player.next();

            // Send response
            msg_mp3_command_response_t response;
            response.success = true;
            response.error_code = 0;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0306: // Previous song
        {
            mp3_player.previous();

            // Send response
            msg_mp3_command_response_t response;
            response.success = true;
            response.error_code = 0;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        case MSG_0307: // Pause or play
        {
            mp3_player.pause_or_play();

            // Send response
            msg_mp3_command_response_t response;
            response.success = true;
            response.error_code = 0;

            msg_t resp_msg;
            resp_msg.msg_id = MSG_0308;
            resp_msg.data_size = sizeof(msg_mp3_command_response_t);
            resp_msg.data_bytes = (u8*)&response;
            messagebroker_publish(&resp_msg);
            break;
        }

        default: break;
    }
}
