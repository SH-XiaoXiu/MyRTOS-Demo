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

#include "MyRTOS_Stream_Def.h"

/**
 * @brief VTS������������������ϡ�
 */
typedef struct {
    /** @brief ��Ҫ����ͨ���������� (��������: VTS -> ��������)��*/
    StreamHandle_t primary_input_stream;

    /** @brief ��Ҫ����ͨ��������� (��������: �������� -> VTS)��*/
    StreamHandle_t primary_output_stream;

    /** @brief ���ڻ㼯����������������ĺ�̨�� (��������: ��̨���� -> VTS)��*/
    StreamHandle_t background_stream;
} VTS_Handles_t;

/**
 * @brief ��ʼ�������������ն˷���
 * @param physical_stream [in] ָ�������ն����ľ����
 * @param handles         [out] ���ڽ���VTS�����������������
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾʧ�ܡ�
 */
int VTS_Init(StreamHandle_t physical_stream, VTS_Handles_t *handles);

/**
 * @brief ���õ�ǰӵ���ն˽�����������
 * @details ֻ�б�����Ϊ���������������ŻᱻVTS·�ɵ������նˡ�
 *          �������������������������
 * @param stream [in] Ҫ����Ϊ������������������ VTS �������κ��������
 *                    ������ NULL ��ȡ�����н��㣨��Ĭģʽ����
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾ��δ��VTS����
 */
int VTS_SetFocus(StreamHandle_t stream);

#endif // MYRTOS_SERVICE_VTS_ENABLE

#endif // MYRTOS_VTS_H