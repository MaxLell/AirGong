#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include "custom_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     * @brief Initialize the WiFiManager module
     */
    void wifimanager_init(void);

    /**
     * @brief Start the WiFiManager task
     */
    void wifimanager_start_task(void);

    /**
     * @brief Check if WiFi is connected
     * @return true if connected, false otherwise
     */
    bool wifimanager_is_connected(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // WIFIMANAGER_H
