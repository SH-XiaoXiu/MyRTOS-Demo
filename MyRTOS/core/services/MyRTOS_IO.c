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


// �����ﶨ��ȫ�ֱ�׼��ָ�롣
// ���ǵĳ�ʼ����ƽ̨���ʵ������ɡ�
Stream_t *g_myrtos_std_in = NULL;
Stream_t *g_myrtos_std_out = NULL;
Stream_t *g_myrtos_std_err = NULL;

//API ʵ��
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

    // �ڵ���������ǰ��StdIO�����ܻ�δ��ƽ̨���ʼ����
    // ��ʱ���˻ص���ԭʼ��ƽ̨PutChar�����������ȷ�����ڵ�������Ϣ�ܱ�������
    if (MyRTOS_Schedule_IsRunning() == 0 || stream == NULL) {
        const char *ptr = buffer;
        while (*ptr) {
            MyRTOS_Platform_PutChar(*ptr++);
        }
    } else {
        // �����������У�����������Ч�ģ�ͨ��������д�롣
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
