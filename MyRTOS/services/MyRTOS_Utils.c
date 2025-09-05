//
// Created by XiaoXiu on 9/5/2025.
//
#include "MyRTOS_Utils.h"
#include <stdio.h>

int MyRTOS_FormatV(char *buffer, size_t size, const char *format, va_list args) {
    if (buffer == NULL || size == 0) {
        return -1;
    }
    int len = vsnprintf(buffer, size, format, args);
    buffer[size - 1] = '\0';
    return len;
}
