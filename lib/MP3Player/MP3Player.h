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
 * @file MP3Player.h
 * @brief MP3 Player module for controlling WT2605C MP3 player via message broker
 *
 * This module provides an interface to control the WT2605C MP3 player module
 * through the message broker system. It supports play mode control, volume control,
 * and playback navigation.
 */

#ifndef MP3PLAYER_H
#define MP3PLAYER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "custom_types.h"

    /**
     * @brief Initialize the MP3 player module
     *
     * Sets up the hardware serial communication with the WT2605C module
     * and subscribes to relevant message broker topics.
     */
    void mp3player_init(void);

    /**
     * @brief Run periodic MP3 player tasks
     *
     * Should be called regularly from the main loop to process
     * any pending operations.
     */
    void mp3player_run(void);

#ifdef __cplusplus
}
#endif

#endif // MP3PLAYER_H
