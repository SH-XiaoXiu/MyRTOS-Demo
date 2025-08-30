//
// Created by XiaoXiu on 9/5/2025.
//

#include "MyRTOS.h"
#include "MyRTOS_IO.h"

#include <stdarg.h>

#include "MyRTOS_Platform.h"
#include <stdio.h>
#include <string.h>

#if (MY_RTOS_USE_STDIO == 1)


// 在这里定义全局标准流指针。
// 它们的初始化由平台层的实现来完成。
Stream_t *g_myrtos_std_in = NULL;
Stream_t *g_myrtos_std_out = NULL;
Stream_t *g_myrtos_std_err = NULL;

//API 实现
int MyRTOS_fprintf(Stream_t *stream, const char *fmt, ...) {
    char buffer[SYS_LOG_MAX_MSG_LENGTH];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len <= 0) {
        return len;
    }

    // 在调度器启动前，StdIO流可能还未被平台层初始化。
    // 此时，退回到最原始的平台PutChar进行输出，以确保早期的启动信息能被看到。
    if (MyRTOS_Schedule_IsRunning() == 0 || stream == NULL) {
        const char *ptr = buffer;
        while (*ptr) {
            MyRTOS_Platform_PutChar(*ptr++);
        }
    } else {
        // 调度器已运行，并且流是有效的，通过流驱动写入。
        if (stream->driver && stream->driver->write) {
            return stream->driver->write(stream, buffer, (size_t) len);
        }
    }

    return len;
}

char *MyRTOS_fgets(char *buffer, int size, Stream_t *stream) {
    if (buffer == NULL || size <= 0 || stream == NULL || stream->driver == NULL || stream->driver->read == NULL) {
        return NULL;
    }

    int pos = 0;
    char c;

    while (pos < size - 1) {
        if (stream->driver->read(stream, &c, 1) != 1) {
            buffer[pos] = '\0';
            return (pos > 0) ? buffer : NULL;
        }

        buffer[pos++] = c;

        if (c == '\n') {
            break;
        }
    }

    buffer[pos] = '\0';
    return buffer;
}


#endif // MY_RTOS_USE_STDIO
