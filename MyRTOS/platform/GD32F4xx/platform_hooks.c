//
// Created by XiaoXiu on 9/1/2025.
//


#include "platform.h"
#include "platform_gd32_console.h"

// �ṩ���й��Ӻ�����Ĭ�Ͽ�ʵ�֣���ʹ�� __weak ����

__attribute__((weak)) void Platform_EarlyInit_Hook(void) {
    // Ĭ�ϲ����κ���
}

__attribute__((weak)) void Platform_BSP_Init_Hook(void) {
    // Ĭ�ϲ����κ���
}

__attribute__((weak)) void Platform_AppSetup_Hook(ShellHandle_t shell_h) {}

__attribute__((weak)) void Platform_CreateTasks_Hook(void) {
    // Ĭ�ϲ����κ���
}

__attribute__((weak)) void Platform_IdleTask_Hook(void *pv) {
    (void) pv;
    for (;;) {
        __WFI(); // �ȴ��жϣ�����͹���ģʽ
    }
}


static void fault_print_string(const char *s) {
    while (*s)
        Platform_fault_putchar(*s++);
}


static void fault_print_hex(uint32_t val) {
    char buffer[11] = "0x";
    char *hex = "0123456789ABCDEF";
    char *p = &buffer[10];
    *p = '\0';
    if (val == 0)
        *--p = '0';
    else
        while (val > 0)
            *--p = hex[val & 0xF], val >>= 4;
    fault_print_string(p);
}

__attribute__((weak)) void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress) {
    // Ĭ��ʵ�֣���ӡ�ؼ��Ĵ�����Ϣ������
    fault_print_string("\r\n--- HARD FAULT ---\r\n");
    fault_print_string("  PC  = ");
    fault_print_hex(pulFaultStackAddress[6]);
    fault_print_string("\r\n  LR  = ");
    fault_print_hex(pulFaultStackAddress[5]);
    fault_print_string("\r\nSystem Halted.\r\n");
    while (1)
        ;
}

__attribute__((weak)) void Platform_StackOverflow_Hook(TaskHandle_t pxTask) {
    fault_print_string("\r\n--- STACK OVERFLOW ---\r\n");
    fault_print_string("  Task Handle: ");
    fault_print_hex((uint32_t) pxTask);
    fault_print_string("\r\nSystem Halted.\r\n");
    fault_print_string("\r\nTask:\r\n");
    fault_print_string(Task_GetName(pxTask));
    while (1)
        ;
}

__attribute__((weak)) void Platform_MallocFailed_Hook(size_t wantedSize) {
    // Ĭ��ʵ�֣���ӡ����Ĵ�С������
    fault_print_string("\r\n--- MALLOC FAILED ---\r\n");
    fault_print_string("  Requested Size: ");
    fault_print_hex((uint32_t) wantedSize);
}
