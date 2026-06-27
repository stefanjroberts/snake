#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *level_name[5] = {"\033[31;40m[FATAL]:\033[0m ", "\033[31m[ERROR]: \033[0m", "\033[33m[WARNING]: \033[0m", "\033[32m[INFO]:\033[0m ",
                             "\033[36m[DEBUG]:\033[0m "};

void platform_log(enum log_level level, const char *message, ...)
{
    char log_output[MAX_LOG_MESSAGE_LENGTH];
    memset(log_output, 0, sizeof(log_output));
    if (level <= CURRENT_LOG_LEVEL)
    {
        va_list arg_ptr;
        va_start(arg_ptr, message);
        vsnprintf(log_output, MAX_LOG_MESSAGE_LENGTH, message, arg_ptr);
        va_end(arg_ptr);

        printf("%s%s\n", level_name[level], log_output);
    }
}

void platform_assert(const char *expression, const char *message, const char *file, int line)
{
    platform_log(platform_FATAL, "ASSERTION FAILED \n\t\t ASSERTION: %s \n\t\t MESSAGE:   %s \n\t\t FILE:      %s \n\t\t LINE:      %d", expression,
                message, file, line);
}
