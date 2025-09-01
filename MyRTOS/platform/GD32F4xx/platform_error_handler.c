//
// Created by XiaoXiu on 9/1/2025.
//

#include "platform.h"
#include "MyRTOS_Extension.h"


/**
 * @brief ����ƽ̨��ע�ᵽ�ں˵�Ψһ�����¼���������
 * @param pEventData �����ں˵��¼����ݡ�
 */
static void platform_kernel_event_handler(const KernelEventData_t *pEventData) {
    switch (pEventData->eventType) {
        case KERNEL_EVENT_HOOK_STACK_OVERFLOW:
            // �ں˱�����ջ���
            // pEventData->task ������Υ������ľ��
            Platform_StackOverflow_Hook(pEventData->task);
            break;

        case KERNEL_EVENT_HOOK_MALLOC_FAILED:
            // �ں˱������ڴ����ʧ��
            // pEventData->mem.size ������������ֽ���
            Platform_MallocFailed_Hook(pEventData->mem.size);
            break;

        case KERNEL_EVENT_ERROR_HARD_FAULT:
            // �ں˱�����Ӳ������
            // pEventData->p_context_data �洢��Ӳ��������Ϣ
            Platform_HardFault_Hook(pEventData->p_context_data);

        // ���������չ�����������¼�
        default:
            break;
    }
}


/**
 * @brief (�ڲ�����) ��ʼ��ƽ̨��Ĵ�������ơ�
 *        �� platform_core.c �� Platform_Init �����е��á�
 */
void Platform_error_handler_init(void) {
    MyRTOS_RegisterExtension(platform_kernel_event_handler);
}
