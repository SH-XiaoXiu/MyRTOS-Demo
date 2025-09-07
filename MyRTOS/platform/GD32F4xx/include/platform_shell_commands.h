//
// Created by XiaoXiu on 9/4/2025.
//

#ifndef MYRTOS_PLATFORM_SHELL_COMMANDS_H
#define MYRTOS_PLATFORM_SHELL_COMMANDS_H


#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include "MyRTOS_Shell.h"


/**
 * @brief 初始化平台程序管理服务。
 */
void Platform_ProgramManager_Init(void);

/**
 * @brief 注册所有平台定义的默认Shell命令。
 * @param shell_h 要注册命令的Shell实例句柄。
 */
void Platform_RegisterDefaultCommands(ShellHandle_t shell_h);

#endif
#endif
