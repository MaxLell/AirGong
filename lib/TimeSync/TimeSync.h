#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <time.h>
#include "custom_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     * @brief Initialize the TimeSync module
     */
    void timesync_init(void);

    /**
     * @brief Start the TimeSync task
     */
    void timesync_start_task(void);

    /**
     * @brief Get the last sync status
     * @return true if time has been synchronized, false otherwise
     */
    bool timesync_is_synchronized(void);

    /**
     * @brief Get current time from either NTP (if available) or RTC
     * @param timeinfo Pointer to tm structure to be filled
     * @return true if time is valid, false otherwise
     */
    bool timesync_get_time(struct tm* timeinfo);

    /**
     * @brief Get current timestamp
     * @return Unix timestamp or 0 if time is not valid
     */
    time_t timesync_get_timestamp(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // TIMESYNC_H
