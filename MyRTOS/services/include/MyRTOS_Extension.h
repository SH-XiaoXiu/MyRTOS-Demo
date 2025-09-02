//
// Created by XiaoXiu on 8/30/2025.
//
#ifndef MYRTOS_EXTENSION_H
#define MYRTOS_EXTENSION_H
#include "MyRTOS.h"

#define MAX_KERNEL_EXTENSIONS 8

/**
 * @brief �ں��¼�����ö��
 *
 * �ں˽��ڹؼ�������㲥��Щ�¼���
 * ��չģ����Լ�����Щ�¼���ִ����Ӧ�Ĺ��ܡ�
 */
typedef enum {
    // ϵͳ�¼�
    KERNEL_EVENT_TICK, // ϵͳTick�жϷ�����
    // �������������¼�
    KERNEL_EVENT_TASK_CREATE, // ���񴴽��ɹ���
    KERNEL_EVENT_TASK_DELETE, // ����ɾ��ǰ
    KERNEL_EVENT_TASK_SWITCH_OUT, // ���񼴽�������
    KERNEL_EVENT_TASK_SWITCH_IN, // ���񼴽�������
    // �ڴ�����¼�
    KERNEL_EVENT_MALLOC, // �ڴ�����
    KERNEL_EVENT_FREE, // �ڴ��ͷź�
    // ������һЩ�����¼�
    KERNEL_EVENT_HOOK_MALLOC_FAILED, // �ڴ����ʧ��
    KERNEL_EVENT_HOOK_STACK_OVERFLOW,
    KERNEL_EVENT_ERROR_HARD_FAULT
    // ����Ҫ��Ӹ����¼� �����һ�ûע���ȥ������
    // KERNEL_EVENT_QUEUE_SEND,
    // KERNEL_EVENT_QUEUE_RECEIVE,
} KernelEventType_t;


/**
 * @brief �ں��¼����������ݽṹ
 *
 * ���ں˹㲥�¼�ʱ���ᴫ��һ��ָ��˽ṹ��ָ�룬
 * ����������¼���ص���������Ϣ��
 */
typedef struct {
    KernelEventType_t eventType; // �¼�����
    TaskHandle_t task; // ������������ (��������������¼�)
    // �����ڴ��¼�
    struct {
        void *ptr; // ����/�ͷŵ��ڴ�ָ��
        size_t size; // ����/��Ĵ�С
    } mem;

    void *p_context_data;
    // ������չ��������������
} KernelEventData_t;


/**
 * @brief �ں���չ�ص���������
 *
 * �ⲿ��չģ����Ҫʵ�ִ����͵ĺ����������ں��¼���
 * @param pEventData ָ������¼���Ϣ�Ľṹ�塣
 */
typedef void (*KernelExtensionCallback_t)(const KernelEventData_t *pEventData);


/**
 * @brief ע��һ���ں���չ
 *
 * �ⲿģ�飨������ʱͳ�ơ����Ը�������ͨ���˺���
 * ���Լ��Ļص�����ע�ᵽ�ںˡ�
 *
 * @param callback Ҫע��Ļص�����ָ�롣
 * @return 0 �ɹ�, -1 ʧ�� (���磬ע�������)��
 */
int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback);

/**
 * @brief ע��һ���ں���չ
 *
 * @param callback Ҫע���Ļص�����ָ�롣
 * @return 0 �ɹ�, -1 ʧ�� (���磬δ�ҵ��ûص�)��
 */
int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback);


#endif // MYRTOS_EXTENSION_H
