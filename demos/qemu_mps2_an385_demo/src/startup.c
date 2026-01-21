/**
 * @brief Startup code for ARM Cortex-M3 (QEMU MPS2-AN385)
 */

#include <stdint.h>

// ============================================================================
//                           外部符号
// ============================================================================

extern uint32_t _estack;        // 栈顶（链接脚本定义）
extern uint32_t _sidata;        // .data 段初始化数据源地址
extern uint32_t _sdata;         // .data 段起始地址
extern uint32_t _edata;         // .data 段结束地址
extern uint32_t _sbss;          // .bss 段起始地址
extern uint32_t _ebss;          // .bss 段结束地址

extern void main(void);
extern void SystemInit(void);

// ============================================================================
//                           异常处理器声明
// ============================================================================

void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void);
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void);
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void);
void SysTick_Handler(void);

// 外设中断处理器
void UART0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART2_Handler(void) __attribute__((weak, alias("Default_Handler")));
void TIMER0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void TIMER1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DUALTIMER_Handler(void) __attribute__((weak, alias("Default_Handler")));

// ============================================================================
//                           向量表
// ============================================================================

__attribute__((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack),     // 初始栈指针
    Reset_Handler,                   // 复位
    NMI_Handler,                     // NMI
    HardFault_Handler,               // 硬故障
    MemManage_Handler,               // 内存管理故障
    BusFault_Handler,                // 总线故障
    UsageFault_Handler,              // 用法故障
    0, 0, 0, 0,                      // 保留
    SVC_Handler,                     // SVC
    DebugMon_Handler,                // 调试监视器
    0,                               // 保留
    PendSV_Handler,                  // PendSV
    SysTick_Handler,                 // SysTick
    // 外设中断
    UART0_Handler,                   // 0: UART0
    UART1_Handler,                   // 1: UART1
    UART2_Handler,                   // 2: UART2
    0, 0, 0, 0, 0,                   // 3-7: 保留
    TIMER0_Handler,                  // 8: TIMER0
    TIMER1_Handler,                  // 9: TIMER1
    DUALTIMER_Handler,               // 10: DUALTIMER
};

// ============================================================================
//                           复位处理器
// ============================================================================

void Reset_Handler(void) {
    uint32_t *src, *dst;

    // 复制 .data 段
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // 清零 .bss 段
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    // 系统初始化
    SystemInit();

    // 调用 main
    main();

    // main 不应该返回，但如果返回则无限循环
    while (1);
}

// ============================================================================
//                           默认中断处理器
// ============================================================================

void Default_Handler(void) {
    while (1);
}
