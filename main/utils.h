#ifndef _UTILS_H
#define _UTILS_H

#if __has_include("sdkconfig.h")
    #include "sdkconfig.h"
#endif

#if defined(__linux__)
    #include <linux/slab.h>
    #include <linux/kernel.h>
    #include <linux/spinlock.h>

    typedef spinlock_t lock_t;

    #define PRINT_LOGO_NAME     "esp32_adapter"

    #define FREE(x)     free(x)

    #define LOCK_INIT(x)   spin_lock_init(x)
    #define LOCK(x)        spin_lock(x)
    #define UNLOCK(x)      spin_unlock(x)

    #define LOG_LEVEL_NONE      0
    #define LOG_LEVEL_ERROR     1
    #define LOG_LEVEL_INFO      2
    #define LOG_LEVEL_DEBUG     3
    #define LOG_LEVEL_TRACE     4

    #define CURRENT_LOG_LEVEL   LOG_LEVEL_INFO

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_TRACE
    #define TRACE_FUNC_ENTRY() \
        printk("[%s] - [%s] : start\n", PRINT_LOGO_NAME, __func__)
    #define TRACE_FUNC_EXIT() \
        printk("[%s] - [%s] : exit\n", PRINT_LOGO_NAME, __func__)
    #else
    #define TRACE_FUNC_ENTRY()
    #define TRACE_FUNC_EXIT()
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        printk("[%s] - [%s] (DEBUG) : " fmt, PRINT_LOGO_NAME, __func__, ##__VA_ARGS__)
    #else
    #define DEBUG_PRINT(fmt, ...)
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO
    #define INFO_PRINT(fmt, ...) \
        printk("[%s] - [%s] (INFO) : " fmt, PRINT_LOGO_NAME, __func__, ##__VA_ARGS__)
    #else
    #define INFO_PRINT(fmt, ...)
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR
    #define ERROR_PRINT(fmt, ...) \
        printk("[%s] - [%s] (ERROR) : " fmt, PRINT_LOGO_NAME, __func__, ##__VA_ARGS__)
    #else
    #define ERROR_PRINT(fmt, ...)
    #endif

#elif defined(CONFIG_IDF_TARGET_ESP32)
    #include <stdlib.h>
    #include <string.h>

    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"
    
    #include "esp_log.h"
    
    typedef SemaphoreHandle_t lock_t;
    typedef uint8_t u8;

    #define LOCK_INIT(x)   *(x) = xSemaphoreCreateMutex()
    #define LOCK(x)        xSemaphoreTake(*(x), portMAX_DELAY)
    #define UNLOCK(x)      xSemaphoreGive(*(x))

    #define PRINT_LOGO_NAME     "esp32_module"

    #define LOG_LEVEL_NONE      0
    #define LOG_LEVEL_ERROR     1
    #define LOG_LEVEL_INFO      2
    #define LOG_LEVEL_DEBUG     3
    #define LOG_LEVEL_TRACE     4

    #define CURRENT_LOG_LEVEL   LOG_LEVEL_INFO

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_TRACE
    #define TRACE_FUNC_ENTRY() \
        ESP_LOGI(PRINT_LOGO_NAME, "[%s] : start", __func__)
    #define TRACE_FUNC_EXIT() \
        ESP_LOGI(PRINT_LOGO_NAME, "[%s] : exit", __func__)
    #else
    #define TRACE_FUNC_ENTRY()
    #define TRACE_FUNC_EXIT()
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        ESP_LOGD(PRINT_LOGO_NAME, "[%s] (DEBUG) : " fmt, __func__, ##__VA_ARGS__)
    #else
    #define DEBUG_PRINT(fmt, ...)
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO
    #define INFO_PRINT(fmt, ...) \
        ESP_LOGI(PRINT_LOGO_NAME, "[%s] (INFO) : " fmt, __func__, ##__VA_ARGS__)
    #else
    #define INFO_PRINT(fmt, ...)
    #endif

    #if CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR
    #define ERROR_PRINT(fmt, ...) \
        ESP_LOGE(PRINT_LOGO_NAME, "[%s] (ERROR) : " fmt, __func__, ##__VA_ARGS__)
    #else
    #define ERROR_PRINT(fmt, ...)
    #endif

#else
    #error "Unknown system!"
#endif /* CONFIG_IDF_TARGET_ESP32 */

#define SAFE_FREE(ptr) \
    do \
    { \
        FREE(ptr); \
        (ptr) = NULL; \
    } while (0)

#endif
