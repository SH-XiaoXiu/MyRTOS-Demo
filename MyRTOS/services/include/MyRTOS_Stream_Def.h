/**
 * @file  MyRTOS_Stream_Def.h
 * @brief MyRTOS IO������ - ���Ķ���
 * @details ���������ӿڵĵײ�ṹ������, ��IO����־��Shell��ģ��Ļ�����
 */

#ifndef MYRTOS_STREAM_DEF_H
#define MYRTOS_STREAM_DEF_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_IO_ENABLE
#define MYRTOS_IO_ENABLE 0
#endif

#ifdef  MYRTOS_SERVICE_IO_ENABLE

#include <stdint.h>
#include <stddef.h>

// ǰ���������Ľṹ��
struct Stream_t;

/** @brief ���ľ����Handle�����ͣ�Ϊָ�����ṹ��Ĳ�͸��ָ�롣 */
typedef struct Stream_t *StreamHandle_t;

/** @brief ���Ķ�����ָ�루Function Pointer�����͹淶�� */
typedef size_t (*StreamReadFn_t)(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

/** @brief ����д����ָ�루Function Pointer�����͹淶�� */
typedef size_t (*StreamWriteFn_t)(StreamHandle_t stream, const void *buffer, size_t bytes_to_write,
                                  uint32_t block_ticks);

/** @brief ���Ŀ��ƺ���ָ�루Function Pointer�����͹淶�� */
typedef int (*StreamControlFn_t)(StreamHandle_t stream, int command, void *arg);

/**
 * @brief ���ӿڣ�Interface���ṹ�嶨�塣
 * @details ����һ��������������������ָ��ġ��麯������������һ��������Ϊ��
 */
typedef struct {
    StreamReadFn_t read;
    StreamWriteFn_t write;
    StreamControlFn_t control;
} StreamInterface_t;

/**
 * @brief ���Ļ��ࣨBase Class���ṹ�塣
 * @details ���о���������󶼱����Դ˽ṹ����Ϊ���һ����Ա����֧��ͳһ�Ľӿڵ��á�
 */
typedef struct Stream_t {
    const StreamInterface_t *p_iface; //ָ��ʵ���˴������ܵĽӿڱ�
    void *p_private_data; //ָ���������˽������
} Stream_t;

#endif

#endif // MYRTOS_STREAM_DEF_H
