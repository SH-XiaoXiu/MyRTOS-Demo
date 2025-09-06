//
// Created by XiaoXiu on 9/1/2025.
//

#include "MyRTOS_Extension.h"
#include "MyRTOS_Port.h"
#include "platform.h"


/**
 * @brief 这是平台层注册到内核的唯一错误事件处理器。
 * @param pEventData 来自内核的事件数据。
 */
static void platform_kernel_event_handler(const KernelEventData_t *pEventData) {
    switch (pEventData->eventType) {
        case KERNEL_EVENT_HOOK_STACK_OVERFLOW: {
            // 内核报告了栈溢出
            // pEventData->task 包含了违规任务的句柄
            Platform_StackOverflow_Hook(pEventData->task);
            break;
        }

        case KERNEL_EVENT_HOOK_MALLOC_FAILED: {
            // 内核报告了内存分配失败
            // pEventData->mem.size 包含了请求的字节数
            Platform_MallocFailed_Hook(pEventData->mem.size);
            break;
        }
        case KERNEL_EVENT_ERROR_HARD_FAULT: {
            // 内核报告了硬件错误
            // pEventData->p_context_data 存储了硬件错误信息
            Platform_HardFault_Hook(pEventData->p_context_data);
        }
        case KERNEL_EVENT_ERROR_TASK_RETURN: {
            // 内核报告了任务返回错误
            PlatformErrorAction_t action = Platform_TaskExit_Hook(pEventData->p_context_data);
            switch (action) {
                case PLATFORM_ERROR_ACTION_HALT: {
                    //请求挂起系统
                    MyRTOS_printf("--- HALT --- \n");
                    MyRTOS_Port_EnterCritical();
                    while (1) {
                    }
                    break; //理论也不会执行到这里
                }

                case PLATFORM_ERROR_ACTION_REBOOT: {
                    //重启
                    MyRTOS_printf("--- SYSTEM REBOOTING ---\n");
                    NVIC_SystemReset();
                    break;
                }
                case PLATFORM_ERROR_ACTION_CONTINUE: {
                    break;
                }
                default: {
                    break;
                }
            }
        }
        // 这里可以扩展处理更多错误事件
        default: {
            break;
        }
    }
}


/**
 * @brief (内部函数) 初始化平台层的错误处理机制。
 *        由 platform_core.c 在 Platform_Init 流程中调用。
 */
void Platform_error_handler_init(void) { MyRTOS_RegisterExtension(platform_kernel_event_handler); }
