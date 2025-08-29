//
// Created by XiaoXiu on 8/29/2025.
//

//����һ����׼��������淶 ����MyRTOSϵͳ ��ݽ׶�.

#if 0
#ifndef MYRTOS_STDIO_H
#define MYRTOS_STDIO_H

#include <stdarg.h>
#include "MyRTOS.h"

// --- ǰ������ ---
struct Stream_t;

// --- �������ӿڶ��� ---
// ����ʵ��һ������ I/O �豸�����ṩ�ĺ�������
typedef struct {
    /**
     * @brief ������д�����ݡ�
     * @param stream ָ���������ָ��
     * @param data Ҫд�������
     * @param length Ҫд��ĳ���
     * @return ʵ��д����ֽ���
     */
    int (*write)(struct Stream_t *stream, const void *data, size_t length);

    /**
     * @brief �����ж�ȡ���ݡ�
     * @param stream ָ���������ָ��
     * @param buffer ���ڴ�Ŷ�ȡ���ݵĻ�����
     * @param length Ҫ��ȡ�ĳ���
     * @return ʵ�ʶ�ȡ���ֽ���
     */
    int (*read)(struct Stream_t *stream, void *buffer, size_t length);

    // (��ѡ) �������ƺ���, �� seek, flush, ioctl ��
    // int (*ioctl)(struct Stream_t *stream, int command, void *arg);
} StreamDriver_t;


// --- ������ṹ�� ---
// һ������ʵ��, ����������������˽������
typedef struct Stream_t {
    const StreamDriver_t *driver; // ָ��ʵ���˾��幦�ܵ�����
    void *private_data; // ָ���豸��ص����� (����: ���ں�, �ļ����)
    // (��ѡ) �����ڴ�������ı�־λ, �� ��/д/׷��, ����/������ ��
} Stream_t;


// --- ȫ�ֱ�׼����� ---
// ��������ϵͳ����Ĭ����, ͨ��ָ�����̨
extern Stream_t *g_myrtos_stdin;
extern Stream_t *g_myrtos_stdout;
extern Stream_t *g_myrtos_stderr;


// ==========================================================
//                 Ӧ�ó���ӿ� (Application API)
// ==========================================================

/**
 * @brief ��ʼ����׼I/Oϵͳ��
 *        ������Ĭ�ϵĿ���̨��, ���� g_myrtos_std* ָ�����ǡ�
 */
void MyRTOS_StdIO_Init(void);


/**
 * @brief ��ʽ�������ָ��������
 * @param stream Ŀ����
 * @param fmt ��ʽ���ַ���
 */
int MyRTOS_fprintf(Stream_t *stream, const char *fmt, ...);

/**
 * @brief ��ʽ���������ǰ����� stdout��
 * @param fmt ��ʽ���ַ���
 */
int MyRTOS_printf(const char *fmt, ...);


/**
 * @brief ��ָ��������ȡ���ݡ�
 * @param stream Դ��
 * @param buffer ������
 * @param length Ҫ��ȡ�ĳ���
 * @return ʵ�ʶ�ȡ���ֽ���
 */
int MyRTOS_fread(Stream_t *stream, void *buffer, size_t length);

/**
 * @brief �ӵ�ǰ����� stdin ��ȡ���ݡ�
 * @param buffer ������
 * @param length Ҫ��ȡ�ĳ���
 * @return ʵ�ʶ�ȡ���ֽ���
 */
int MyRTOS_read(void *buffer, size_t length);


/**
 * @brief �ض�������ı�׼����
 * @param task_h Ŀ�������� (NULL ��ʾ��ǰ����)
 * @param std_stream_id 0 for stdin, 1 for stdout, 2 for stderr
 * @param new_stream Ҫ�ض��򵽵�����
 * @return 0 �ɹ�, -1 ʧ��
 */
int MyRTOS_Task_RedirectStdStream(TaskHandle_t task_h, int std_stream_id, Stream_t *new_stream);


#endif // MYRTOS_STDIO_H
#endif
