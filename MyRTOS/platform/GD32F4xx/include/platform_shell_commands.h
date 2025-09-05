//
// Created by XiaoXiu on 9/4/2025.
//

#ifndef MYRTOS_PLATFORM_SHELL_COMMANDS_H
#define MYRTOS_PLATFORM_SHELL_COMMANDS_H

#include "platform.h"

#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include <stdbool.h>
#include "MyRTOS_Shell.h"

//=================================
// 平台程序管理 (Program Management)
//=================================
#if PLATFORM_USE_PROGRAM_MANGE == 1

// 定义程序入口函数原型
typedef int (*ProgramMain_t)(int argc, char *argv[]);

// 全局程序表条目结构体
typedef struct {
    const char *name;
    const char *description;
    ProgramMain_t main_func;
} ProgramEntry_t;

/**
 * @brief 初始化平台程序管理服务。
 */
void Platform_ProgramManager_Init(void);

/**
 * @brief 检查当前任务是否应该关闭。
 * @details 允许长时间运行的程序优雅地退出。
 * @return bool true表示应关闭，false表示继续运行。
 */
bool Program_ShouldShutdown(void);

#endif // PLATFORM_USE_PROGRAM_MANGE


//===============
// Shell 命令注册
//===============

/**
 * @brief 注册所有平台定义的默认Shell命令。
 * @param shell_h 要注册命令的Shell实例句柄。
 */
void Platform_RegisterDefaultCommands(ShellHandle_t shell_h);

#endif
#endif
