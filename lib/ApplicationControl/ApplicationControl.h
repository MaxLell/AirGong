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
 * @file ApplicationControl.h
 * @brief Application Control module for scheduled song playback
 *
 * This module manages scheduled song playback at specific times.
 * Users can configure multiple time-based triggers via the console.
 */

#ifndef APPLICATIONCONTROL_H
#define APPLICATIONCONTROL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "custom_types.h"

    /**
     * @brief Initialize the Application Control module
     *
     * Sets up the module and subscribes to relevant message broker topics.
     */
    void appcontrol_init(void);

    /**
     * @brief Run periodic Application Control tasks
     *
     * Should be called regularly from the main loop to check scheduled
     * events and trigger song playback when needed.
     */
    void appcontrol_run(void);

#ifdef __cplusplus
}
#endif

#endif // APPLICATIONCONTROL_H
