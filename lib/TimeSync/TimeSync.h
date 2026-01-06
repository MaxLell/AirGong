#ifndef TIMESYNC_H
#define TIMESYNC_H

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // TIMESYNC_H
