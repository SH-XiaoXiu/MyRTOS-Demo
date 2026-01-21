//
// GD32F407 Demo Platform Error Handler
//
// 内核错误事件处理器
//

#include "platform.h"
#include "MyRTOS_Extension.h"
#include "MyRTOS_Port.h"

#if MYRTOS_SERVICE_IO_ENABLE == 1
#include "MyRTOS_IO.h"
#endif

/**
 * @brief 平台层注册到内核的错误事件处理器
 * @param pEventData 来自内核的事件数据
 */
static void platform_kernel_event_handler(const KernelEventData_t *pEventData) {
    switch (pEventData->eventType) {
        case KERNEL_EVENT_HOOK_STACK_OVERFLOW: {
            // 内核报告了栈溢出
            Platform_StackOverflow_Hook(pEventData->task);
            break;
        }

        case KERNEL_EVENT_HOOK_MALLOC_FAILED: {
            // 内核报告了内存分配失败
            Platform_MallocFailed_Hook(pEventData->mem.size);
            break;
        }

        case KERNEL_EVENT_ERROR_HARD_FAULT: {
            // 内核报告了硬件错误
            Platform_HardFault_Hook(pEventData->p_context_data);
            break;
        }

        case KERNEL_EVENT_ERROR_TASK_RETURN: {
            // 内核报告了任务返回错误
            PlatformErrorAction_t action = Platform_TaskExit_Hook(pEventData->p_context_data);
            switch (action) {
                case PLATFORM_ERROR_ACTION_HALT: {
                    // 请求挂起系统
#if MYRTOS_SERVICE_IO_ENABLE == 1
                    MyRTOS_printf("--- HALT --- \n");
#endif
                    MyRTOS_Port_EnterCritical();
                    while (1) {
                    }
                    break;
                }

                case PLATFORM_ERROR_ACTION_REBOOT: {
                    // 重启
#if MYRTOS_SERVICE_IO_ENABLE == 1
                    MyRTOS_printf("--- SYSTEM REBOOTING ---\n");
#endif
                    Platform_Reboot();
                    break;
                }

                case PLATFORM_ERROR_ACTION_CONTINUE:
                default: {
                    break;
                }
            }
            break;
        }

        default: {
            break;
        }
    }
}

/**
 * @brief 初始化平台层的错误处理机制
 *        应在内核初始化后调用
 */
void Platform_ErrorHandler_Init(void) {
    MyRTOS_RegisterExtension(platform_kernel_event_handler);
}
