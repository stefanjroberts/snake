#pragma once
#include "defines.h"
// NOTE: Global Parameters
#define MAX_LOG_MESSAGE_LENGTH 2000
#define platform_ASSERT 1

#ifdef BUILD_DEBUG
#define CURRENT_LOG_LEVEL 4
#define platform_ASSERT_DEBUG 1
#else
#define CURRENT_LOG_LEVEL 2
#define platform_ASSERT_DEBUG 0
#endif

// NOTE: Logging Macros

#define PFATAL(message, ...) platform_log(platform_FATAL, message, ##__VA_ARGS__);
#define PERROR(message, ...) platform_log(platform_ERROR, message, ##__VA_ARGS__);

#if CURRENT_LOG_LEVEL > 1
#define PWARN(message, ...) platform_log(platform_WARN, message, ##__VA_ARGS__);
#else
#define PWARN(message, ...)
#endif

#if CURRENT_LOG_LEVEL > 2
#define PINFO(message, ...) platform_log(platform_INFO, message, ##__VA_ARGS__);
#else
#define PINFO(message, ...)
#endif

#if CURRENT_LOG_LEVEL > 3
#define PDEBUG(message, ...) platform_log(platform_DEBUG, message, ##__VA_ARGS__);
#else
#define PDEBUG(message, ...)
#endif

// NOTE: Assertion Macros

#if platform_ASSERT == 1
#define PASSERT(expr, msg)                                                                                                                           \
    if (!(expr))                                                                                                                                     \
    {                                                                                                                                                \
        platform_assert(#expr, msg, __FILE__, __LINE__);                                                                                             \
        __builtin_trap();                                                                                                                            \
    }
#else
#define PASSERT(expr)                                                                                                                                \
    if (expr)                                                                                                                                        \
    {                                                                                                                                                \
    }
#define PASSERT_MSG(expr, msg)                                                                                                                       \
    if (expr)                                                                                                                                        \
    {                                                                                                                                                \
    }
#endif

#if platform_ASSERT_DEBUG == 1
#define PASSERT_DBG(expr, msg)                                                                                                                       \
    if (!(expr))                                                                                                                                     \
    {                                                                                                                                                \
        platform_assert(#expr, msg, __FILE__, __LINE__);                                                                                             \
        __builtin_trap();                                                                                                                            \
    }
#else
#define PASSERT_DBG(expr, msg)                                                                                                                       \
    if (expr)                                                                                                                                        \
    {                                                                                                                                                \
    }
#endif

// NOTE: Logger Functions
enum log_level
{
    platform_FATAL = 0,
    platform_ERROR = 1,
    platform_WARN = 2,
    platform_INFO = 3,
    platform_DEBUG = 4
};

void platform_log(enum log_level level, const char *message, ...);
void platform_assert(const char *expression, const char *message, const char *file, int line);
