//
// GD32F407 Demo Platform Hooks
//
// 平台钩子函数的默认实现（weak）
// 用户可以在自己的代码中重写这些函数
//

#include "platform.h"

// ============================================================================
//                           生命周期钩子
// ============================================================================

__attribute__((weak)) void Platform_EarlyInit_Hook(void) {
    // 默认不做任何事
}

__attribute__((weak)) void Platform_BSP_Init_Hook(void) {
    // 默认不做任何事
}

__attribute__((weak)) void Platform_BSP_After_Hook(void) {
    // 默认不做任何事
}

__attribute__((weak)) void Platform_AppSetup_Hook(void) {
    // 默认不做任何事
}

__attribute__((weak)) void Platform_CreateTasks_Hook(void) {
    // 默认不做任何事
}

__attribute__((weak)) void Platform_IdleTask_Hook(void *pv) {
    (void) pv;
    for (;;) {
        __WFI(); // 等待中断，进入低功耗模式
    }
}

// ============================================================================
//                           错误处理辅助函数
// ============================================================================

static void fault_print_string(const char *s) {
    while (*s) {
        Platform_fault_putchar(*s++);
    }
}

static void fault_print_hex(uint32_t val) {
    char buffer[11] = "0x";
    char *hex = "0123456789ABCDEF";
    char *p = &buffer[10];
    *p = '\0';
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            *--p = hex[val & 0xF];
            val >>= 4;
        }
    }
    fault_print_string(p);
}

// ============================================================================
//                           错误处理钩子
// ============================================================================

__attribute__((weak)) void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress) {
    // 默认实现：打印关键寄存器信息并挂起
    fault_print_string("\r\n--- HARD FAULT ---\r\n");
    fault_print_string("  PC  = ");
    fault_print_hex(pulFaultStackAddress[6]);
    fault_print_string("\r\n  LR  = ");
    fault_print_hex(pulFaultStackAddress[5]);
    fault_print_string("\r\nSystem Halted.\r\n");
    while (1);
}

__attribute__((weak)) void Platform_StackOverflow_Hook(TaskHandle_t pxTask) {
    fault_print_string("\r\n--- STACK OVERFLOW ---\r\n");
    fault_print_string("  Task Handle: ");
    fault_print_hex((uint32_t) pxTask);
    fault_print_string("\r\nSystem Halted.\r\n");
    fault_print_string("\r\nTask:\r\n");
    fault_print_string(Task_GetName(pxTask));
    while (1);
}

__attribute__((weak)) PlatformErrorAction_t Platform_TaskExit_Hook(TaskHandle_t pxTask) {
    fault_print_string("\r\n--- TASK EXIT ---\r\n");
    fault_print_string("  Task Handle: ");
    fault_print_hex((uint32_t) pxTask);
    fault_print_string("\r\nTask:\r\n");
    fault_print_string(Task_GetName(pxTask));
    return PLATFORM_ERROR_ACTION_HALT;
}

__attribute__((weak)) void Platform_MallocFailed_Hook(size_t wantedSize) {
    // 默认实现：打印请求的大小并挂起
    fault_print_string("\r\n--- MALLOC FAILED ---\r\n");
    fault_print_string("  Requested Size: ");
    fault_print_hex((uint32_t) wantedSize);
}
