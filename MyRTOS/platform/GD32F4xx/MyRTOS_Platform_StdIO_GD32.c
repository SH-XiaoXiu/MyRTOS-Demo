//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: GD32F4xx Series
//

#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Platform.h"

#if (MY_RTOS_USE_STDIO == 1)

// --- 平台相关的驱动函数实现 ---
static int prvSerialStream_Write_GD32(Stream_t *stream, const void *data, size_t length);

static int prvSerialStream_Read_GD32(Stream_t *stream, void *buffer, size_t length);

static int prvSerialStream_Ioctl_GD32(Stream_t *stream, int command, void *arg);


// GD32平台的默认串口流驱动实例
static const StreamDriver_t g_serial_driver_gd32 = {
    .write = prvSerialStream_Write_GD32,
    .read = prvSerialStream_Read_GD32,
    .ioctl = prvSerialStream_Ioctl_GD32,
};

// GD32平台的默认串口流实例
static Stream_t g_serial_stream_instance_gd32 = {
    .driver = &g_serial_driver_gd32,
    .private_data = NULL, // 在这个简单的实现中，我们不需要私有数据
};


/**
 * @brief 初始化StdIO系统 (平台特定部分)
 *        这个函数由通用的 MyRTOS_SystemInit 调用。
 *        它的职责是创建特定于本平台的流实例，并将其挂接到
 *        StdIO服务层的全局指针上。
 */
void MyRTOS_StdIO_Init(void) {
    // 将StdIO服务层的全局流指针，指向本平台创建的串口流实例
    g_myrtos_std_in = &g_serial_stream_instance_gd32;
    g_myrtos_std_out = &g_serial_stream_instance_gd32;
    g_myrtos_std_err = &g_serial_stream_instance_gd32;
}

// --- 驱动函数的具体实现 ---

static int prvSerialStream_Write_GD32(struct Stream_t *stream, const void *data, size_t length) {
    (void) stream; // 在此实现中未使用
    const char *ptr = (const char *) data;
    for (size_t i = 0; i < length; i++) {
        MyRTOS_Platform_PutChar(ptr[i]);
    }
    return (int) length;
}

static int prvSerialStream_Read_GD32(struct Stream_t *stream, void *buffer, size_t length) {
    (void) stream; // 在此实现中未使用
    char *char_buf = buffer;
    for (size_t i = 0; i < length; i++) {
        if (MyRTOS_Platform_GetChar_Blocking(&char_buf[i]) != 1) {
            return (int) i;
        }
    }
    return (int) length;
}

static int prvSerialStream_Ioctl_GD32(Stream_t *stream, int command, void *arg) {
    (void) stream;
    (void) command;
    (void) arg;
    return -1; // 不支持任何命令
}


#endif // MY_RTOS_USE_STDIO
