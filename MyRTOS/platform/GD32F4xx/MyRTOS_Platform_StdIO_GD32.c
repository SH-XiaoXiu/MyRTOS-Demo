//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: GD32F4xx Series
//

#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Platform.h"

#if (MY_RTOS_USE_STDIO == 1)

// --- ƽ̨��ص���������ʵ�� ---
static int prvSerialStream_Write_GD32(Stream_t *stream, const void *data, size_t length);

static int prvSerialStream_Read_GD32(Stream_t *stream, void *buffer, size_t length);

static int prvSerialStream_Ioctl_GD32(Stream_t *stream, int command, void *arg);


// GD32ƽ̨��Ĭ�ϴ���������ʵ��
static const StreamDriver_t g_serial_driver_gd32 = {
    .write = prvSerialStream_Write_GD32,
    .read = prvSerialStream_Read_GD32,
    .ioctl = prvSerialStream_Ioctl_GD32,
};

// GD32ƽ̨��Ĭ�ϴ�����ʵ��
static Stream_t g_serial_stream_instance_gd32 = {
    .driver = &g_serial_driver_gd32,
    .private_data = NULL, // ������򵥵�ʵ���У����ǲ���Ҫ˽������
};


/**
 * @brief ��ʼ��StdIOϵͳ (ƽ̨�ض�����)
 *        ���������ͨ�õ� MyRTOS_SystemInit ���á�
 *        ����ְ���Ǵ����ض��ڱ�ƽ̨����ʵ����������ҽӵ�
 *        StdIO������ȫ��ָ���ϡ�
 */
void MyRTOS_StdIO_Init(void) {
    // ��StdIO������ȫ����ָ�룬ָ��ƽ̨�����Ĵ�����ʵ��
    g_myrtos_std_in = &g_serial_stream_instance_gd32;
    g_myrtos_std_out = &g_serial_stream_instance_gd32;
    g_myrtos_std_err = &g_serial_stream_instance_gd32;
}

// --- ���������ľ���ʵ�� ---

static int prvSerialStream_Write_GD32(struct Stream_t *stream, const void *data, size_t length) {
    (void) stream; // �ڴ�ʵ����δʹ��
    const char *ptr = (const char *) data;
    for (size_t i = 0; i < length; i++) {
        MyRTOS_Platform_PutChar(ptr[i]);
    }
    return (int) length;
}

static int prvSerialStream_Read_GD32(struct Stream_t *stream, void *buffer, size_t length) {
    (void) stream; // �ڴ�ʵ����δʹ��
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
    return -1; // ��֧���κ�����
}


#endif // MY_RTOS_USE_STDIO
