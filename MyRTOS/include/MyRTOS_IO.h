#ifndef MYRTOS_IO_H
#define MYRTOS_IO_H

#include "MyRTOS_Config.h"
#include <stddef.h>

#if (MY_RTOS_USE_STDIO == 1)

// �����ڲ�ͬ�ṹ���л�������ָ��
struct Stream_t;
struct Task_t;

// --- �������ӿڶ��� ---
// ����һ�� I/O �豸������Ҫʵ�ֵĺ������ϣ�������StdIOϵͳ
typedef struct {
    /**
     * @brief ������д�����ݡ�
     * @param stream ָ���������ָ��
     * @param data Ҫд�������
     * @param length Ҫд����ֽ���
     * @return ʵ��д����ֽ���
     */
    int (*write)(struct Stream_t *stream, const void *data, size_t length);

    /**
     * @brief �����ж�ȡ���ݡ�
     * @param stream ָ���������ָ��
     * @param buffer ���ڴ�Ŷ�ȡ���ݵĻ�����
     * @param length ϣ����ȡ���ֽ���
     * @return ʵ�ʶ�ȡ���ֽ���
     */
    int (*read)(struct Stream_t *stream, void *buffer, size_t length);

    /**
     * @brief (��ѡ) �豸���ƽӿڣ������ض�������
     * @param stream ָ���������ָ��
     * @param command ��������ID
     * @param arg ������صĲ���
     * @return ����ִ�еĽ��
     */
    int (*ioctl)(struct Stream_t *stream, int command, void *arg);
} StreamDriver_t;


// --- ������ṹ�� ---
// һ������ʵ����������������;����Ӳ��ʵ������һ��
typedef struct Stream_t {
    const StreamDriver_t *driver; // ָ��ʵ���˹��ܵ�����
    void *private_data; // ָ���豸��ص����� (����: ���ھ��, �ļ�������)
} Stream_t;


// --- ȫ�ֱ�׼����� ---
// ϵͳ����Ĭ��������������StdIO��ʼ��ʱָ��Ĭ�ϵĿ���̨
extern Stream_t *g_myrtos_std_in;
extern Stream_t *g_myrtos_std_out;
extern Stream_t *g_myrtos_std_err;

// ==========================================================
//                 Ӧ�ó���ӿ� (Application API)
// ==========================================================

/**
 * @brief ��ʼ����׼I/Oϵͳ��
 *        ����Ĭ�ϵ�ϵͳ��, ���� g_myrtos_std* ָ�����ǡ�
 */
void MyRTOS_StdIO_Init(void);

/**
 * @brief ��ʽ�������ָ��������
 *        �������и�ʽ����������Ļ�����
 * @param stream Ŀ����
 * @param fmt ��ʽ���ַ���
 * @param ... �ɱ����
 * @return �ɹ�д����ַ���
 */
int MyRTOS_fprintf(Stream_t *stream, const char *fmt, ...);

/**
 * @brief ��ʽ���������ǰ����ı�׼���(stdout)��
 */
#define MyRTOS_printf(...) MyRTOS_fprintf(Task_GetStdOut(NULL), __VA_ARGS__)

/**
 * @brief ��ָ��������ȡһ��(��'\n'��β)���ַ�����
 *        ������ֱ����ȡ�����з��򻺳�������
 * @param buffer �洢�ַ����Ļ�����
 * @param size ������������С
 * @param stream Դ��
 * @return �ɹ��򷵻�ָ�򻺳�����ָ��, ʧ�ܻ�EOF�򷵻�NULL��
 */
char *MyRTOS_fgets(char *buffer, int size, Stream_t *stream);

/**
 * @brief �ӵ�ǰ����ı�׼����(stdin)��ȡһ�С�
 */
#define MyRTOS_gets(buf, size) MyRTOS_fgets(buf, size, Task_GetStdIn(NULL))

#else
#define MyRTOS_printf(...) ((void)0)
#define MyRTOS_fprintf(stream, fmt, ...) (0)
#define MyRTOS_gets(buf, size) (NULL)
#endif // MY_RTOS_USE_STDIO
#endif // MYRTOS_IO_H
