/**
 * @file  init.h
 * @brief Init进程接口定义
 */
#ifndef INIT_H
#define INIT_H

#include "MyRTOS_Service_Config.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"

/**
 * @brief Init程序定义
 * @details 系统第一个用户进程，负责启动Shell
 */
extern const ProgramDefinition_t g_program_init;

#endif // MYRTOS_SERVICE_PROCESS_ENABLE

#endif // INIT_H
