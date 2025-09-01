/**
 * @file  MyRTOS_IO.h
 * @brief MyRTOS IO������ - �����ӿ�
 * @details �ṩ�����׼IO�ض�����ʽ��д���ܵ�(Pipe)�ȹ��ܡ�
 */
#ifndef MYRTOS_IO_H
#define MYRTOS_IO_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_IO_ENABLE
#define MYRTOS_IO_ENABLE 0
#endif

#if MYRTOS_SERVICE_IO_ENABLE == 1


#include "MyRTOS.h"
#include "MyRTOS_Stream_Def.h"
#include <stdarg.h>


/*================================== ��׼�� API ==================================*/

/**
 * @brief ��ʼ����׼IO������չ��
 * @details �����ڴ����κ�����֮ǰ���ã���ע���ں��¼���������
 *          ȷ�������ܹ���ȷ�̳и�����ı�׼����
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾʧ�ܡ�
 */
int StdIOService_Init(void);

/**
 * @brief ��ȡָ������ı�׼��������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return StreamHandle_t ָ�������stdin���ľ��
 */
StreamHandle_t Task_GetStdIn(TaskHandle_t task_h);

/**
 * @brief ��ȡָ������ı�׼�������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return StreamHandle_t ָ�������stdout���ľ��
 */
StreamHandle_t Task_GetStdOut(TaskHandle_t task_h);

/**
 * @brief ��ȡָ������ı�׼��������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return StreamHandle_t ָ�������stderr���ľ��
 */
StreamHandle_t Task_GetStdErr(TaskHandle_t task_h);

/**
 * @brief �ض�������ı�׼��������
 * @param task_h Ҫ������������ (NULL ��ʾ��ǰ����)
 * @param new_stdin �µı�׼���������
 */
void Task_SetStdIn(TaskHandle_t task_h, StreamHandle_t new_stdin);

/**
 * @brief �ض�������ı�׼�������
 * @param task_h Ҫ������������ (NULL ��ʾ��ǰ����)
 * @param new_stdout �µı�׼��������
 */
void Task_SetStdOut(TaskHandle_t task_h, StreamHandle_t new_stdout);

/**
 * @brief �ض�������ı�׼��������
 * @param task_h Ҫ������������ (NULL ��ʾ��ǰ����)
 * @param new_stderr �µı�׼���������
 */
void Task_SetStdErr(TaskHandle_t task_h, StreamHandle_t new_stderr);

/*================================ ��ʽ I/O ���� API ===============================*/

/**
 * @brief ��ָ�������ж�ȡ���ݡ�
 * @param stream        [in] �����
 * @param buffer        [out] ��Ŷ�ȡ���ݵĻ�����
 * @param bytes_to_read [in] ϣ����ȡ���ֽ���
 * @param block_ticks   [in] �����ȴ�ʱ�� (ticks)
 * @return size_t ʵ�ʶ�ȡ�����ֽ���
 */
size_t Stream_Read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

/**
 * @brief ��ָ��������д�����ݡ�
 * @param stream         [in] �����
 * @param buffer         [in] ��д�����ݵĻ�����
 * @param bytes_to_write [in] ϣ��д����ֽ���
 * @param block_ticks    [in] �����ȴ�ʱ�� (ticks)
 * @return size_t ʵ��д����ֽ���
 */
size_t Stream_Write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks);

/**
 * @brief ��ָ��������д���ʽ���ַ�����
 * @param stream [in] �����
 * @param format [in] ��ʽ���ַ���
 * @param ...    [in] �ɱ����
 * @return int �ɹ�д����ַ���
 */
int Stream_Printf(StreamHandle_t stream, const char *format, ...);

/**
 * @brief Stream_Printf��va_list�汾��
 */
int Stream_VPrintf(StreamHandle_t stream, const char *format, va_list args);

// Ĭ�ϲ�����ǰ����ı�׼���ı�ݺ�
#define MyRTOS_printf(format, ...) Stream_Printf(Task_GetStdOut(NULL), format, ##__VA_ARGS__)
#define MyRTOS_putchar(c)          Stream_Write(Task_GetStdOut(NULL), &(c), 1, MYRTOS_MAX_DELAY)
#define MyRTOS_getchar()           ({ char __ch; Stream_Read(Task_GetStdIn(NULL), &__ch, 1, MYRTOS_MAX_DELAY); __ch; })

/*============================== Pipe (�����ͨ����) API =============================*/

/**
 * @brief ����һ���ܵ���Pipe����
 * @details �ܵ���һ���ں˹����FIFO�ֽڻ���������ʵ�������ӿڣ�
 *          ����������һ�������stdout����һ�������stdin��ʵ�������ͨ�š�
 * @param buffer_size �ܵ��ڲ��������Ĵ�С���ֽڣ���
 * @return StreamHandle_t �ɹ��򷵻عܵ����ľ����ʧ���򷵻� NULL��
 */
StreamHandle_t Pipe_Create(size_t buffer_size);

/**
 * @brief ɾ��һ���ܵ���
 * @param pipe_stream Ҫɾ���Ĺܵ��������
 */
void Pipe_Delete(StreamHandle_t pipe_stream);

#endif // MYRTOS_IO_ENABLE

#endif // MYRTOS_IO_H
