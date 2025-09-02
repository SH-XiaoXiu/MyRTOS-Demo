#if 0
#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H


// 平台层会自动包含MyRTOS的配置文件，以决定需要初始化哪些服务。
// 请确保你的项目包含路径正确设置，可以找到这两个文件。
#include "MyRTOS_Config.h"
#include "MyRTOS_Service_Config.h"

// =========================================================================
//                   可执行程序 配置
// =========================================================================
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
#define PLATFORM_PROGRAM_LAUNCH_STACK 64 // 启动任务栈大小
#endif


// =========================================================================
//                   调试控制台 (Console) 配置
// =========================================================================
#define PLATFORM_USE_CONSOLE 1 // 自动使能平台控制台

//选择使用的USART外设 作为全局标准IO
#define PLATFORM_CONSOLE_USART_NUM 0
//波特率
#define PLATFORM_CONSOLE_BAUDRATE 115200U

//中断和缓冲区 ---
#define PLATFORM_CONSOLE_RX_BUFFER_SIZE 128
#define PLATFORM_CONSOLE_IRQ_PRIORITY 5


// =========================================================================
//                   高精度定时器 (Monitor Service) 配置
// =========================================================================
#if (MYRTOS_SERVICE_MONITOR_ENABLE == 1)
#define PLATFORM_USE_HIRES_TIMER 1 // 自动使能高精度定时器

//选择用作高精度时基的通用定时器 ---
// 推荐使用32位定时器，如 TIMER1, TIMER2, TIMER3, TIMER4 (在GD32中对应宏为1,2,3,4)
#define PLATFORM_HIRES_TIMER_NUM 1

//频率 (Hz) ---
// 1MHz (1 tick = 1us) 是一个很好的选择，便于调试
#define PLATFORM_HIRES_TIMER_FREQ_HZ 1000000U
#else
#define PLATFORM_USE_HIRES_TIMER 0
#endif

// =========================================================================
//                   平台 Hook  配置
// =========================================================================
#define PLATFORM_USE_ERROR_HOOK 1

// =========================================================================
//                   平台 Shell  配置
// =========================================================================
#if MYRTOS_SERVICE_SHELL_ENABLE == 1
#define PLATFORM_USE_DEFAULT_COMMANDS 1
#define PLATFORM_USE_PROGRAM_MANGE 1
#define MAX_REGISTERED_PROGRAMS 16
#else
#define PLATFORM_USE_DEFAULT_COMMANDS 0
#define PLATFORM_USE_PROGRAM_MANGE 0
#endif


#endif // PLATFORM_CONFIG_H


#endif
