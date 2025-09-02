/**
 * @file  MyRTOS_VTS.h
 * @brief MyRTOS �����ն˷���
 * @details ���������ն˵�IO·�ɵ���ǰӵ�С����㡱����������
 *          ʵ��������ǰ̨/��̨���̵Ķ�ռʽ�ն˷���ģ�͡�
 */
#ifndef MYRTOS_VTS_H
#define MYRTOS_VTS_H

#include "MyRTOS_Service_Config.h"

#ifndef MYRTOS_SERVICE_VTS_ENABLE
#define MYRTOS_SERVICE_VTS_ENABLE 0
#endif
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
#include <stdbool.h>
#include <stddef.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"


/**
 * @brief ��VTS����'back'�������õĻص�����ԭ�͡�
 * @details ƽ̨��Ӧ�ڴ˻ص���ʵ������ǰ̨������߼���
 */
typedef void (*VTS_BackCommandCallback_t)(void);

/**
 * @brief VTS ��ʼ�����ýṹ��
 */
typedef struct {
    StreamHandle_t physical_stream; // ����I/O�� (����: UART��)
    StreamHandle_t root_input_stream; // VTS�����з�'back'����ת�������� (ͨ����Shell������ܵ�)
    StreamHandle_t root_output_stream; // ��������������Ҳ��'back'������Ĭ�Ͻ���
    const char *back_command_sequence; // ���ڴ����������õ��������� (����: "back\r\n")
    size_t back_command_len; // back�������еĳ���
    VTS_BackCommandCallback_t on_back_command; // ��'back'����ɹ�ִ��ʱ���õĻص�
} VTS_Config_t;

/**
 * @brief ��ʼ�������������ն˷���
 * @details �˺����ᴴ�����б�����ڲ��ܵ���VTS������
 * @param config [in] ָ��VTS���ýṹ���ָ�롣
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾʧ�� (�����ڴ治��)��
 */
int VTS_Init(const VTS_Config_t *config);

/**
 * @brief ���õ�ǰ���ն˽��㡣
 * @details �������ն˵�����л���ָ��������
 * @param output_stream [in] �µĽ����������
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾVTSδ��ʼ����
 */
int VTS_SetFocus(StreamHandle_t output_stream);

/**
 * @brief ���������ûظ���Shell������
 * @details ͨ����'shell'������á��˺������ᴥ�� on_back_command �ص���
 */
void VTS_ReturnToRootFocus(void);

/**
 * @brief ���û�ȡ����ȫ����־ģʽ����
 * @details �ڴ�ģʽ�£�VTS�᳢�Դ�������֪������������̨����ǰ���㣩��ȡ�������ӡ�������նˡ�
 * @param enable [in] true ����ȫ����־ģʽ, false ���á�
 */
void VTS_SetLogAllMode(bool enable);

/**
 * @brief ��ȡVTS�����ĺ�̨�������
 * @details ���в����ն�ֱ�ӽ����ĺ�̨����Ӧ����stdout/stderr�ض��򵽴�����
 * @return StreamHandle_t ��̨���ľ�������VTSδ��ʼ���򷵻�NULL��
 */
StreamHandle_t VTS_GetBackgroundStream(void);

#endif // MYRTOS_VTS_H
#endif
