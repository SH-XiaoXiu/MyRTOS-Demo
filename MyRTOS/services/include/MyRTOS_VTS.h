/**
 * @file  MyRTOS_VTS.h
 * @brief MyRTOS 虚拟终端服务
 * @details 负责将物理终端的IO路由到当前拥有“焦点”的虚拟流。
 *          实现了类似前台/后台进程的独占式终端访问模型。
 */
#ifndef MYRTOS_VTS_H
#define MYRTOS_VTS_H

#include "MyRTOS_Service_Config.h"

#ifndef MYRTOS_SERVICE_VTS_ENABLE
#define MYRTOS_SERVICE_VTS_ENABLE 0
#endif
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
#include <stdbool.h>
#include <stddef.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"


/**
 * @brief 当VTS处理'back'命令后调用的回调函数原型。
 * @details 平台层应在此回调中实现清理前台任务的逻辑。
 */
typedef void (*VTS_BackCommandCallback_t)(void);

/**
 * @brief VTS 初始化配置结构体
 */
typedef struct {
    StreamHandle_t physical_stream; // 物理I/O流 (例如: UART流)
    StreamHandle_t root_input_stream; // VTS将所有非'back'输入转发到此流 (通常是Shell的输入管道)
    StreamHandle_t root_output_stream; // 根任务的输出流，也是'back'命令后的默认焦点
    const char *back_command_sequence; // 用于触发焦点重置的命令序列 (例如: "back\r\n")
    size_t back_command_len; // back命令序列的长度
    VTS_BackCommandCallback_t on_back_command; // 当'back'命令成功执行时调用的回调
} VTS_Config_t;

/**
 * @brief 初始化并启动虚拟终端服务。
 * @details 此函数会创建所有必需的内部管道和VTS主任务。
 * @param config [in] 指向VTS配置结构体的指针。
 * @return int 0 表示成功, -1 表示失败 (例如内存不足)。
 */
int VTS_Init(const VTS_Config_t *config);

/**
 * @brief 设置当前的终端焦点。
 * @details 将物理终端的输出切换到指定的流。
 * @param output_stream [in] 新的焦点输出流。
 * @return int 0 表示成功, -1 表示VTS未初始化。
 */
int VTS_SetFocus(StreamHandle_t output_stream);

/**
 * @brief 将焦点重置回根（Shell）流。
 * @details 通常由'shell'命令调用。此函数不会触发 on_back_command 回调。
 */
void VTS_ReturnToRootFocus(void);

/**
 * @brief 设置或取消“全局日志模式”。
 * @details 在此模式下，VTS会尝试从所有已知的流（根、后台、当前焦点）读取输出并打印到物理终端。
 * @param enable [in] true 启用全局日志模式, false 禁用。
 */
void VTS_SetLogAllMode(bool enable);

/**
 * @brief 获取VTS创建的后台流句柄。
 * @details 所有不与终端直接交互的后台任务应将其stdout/stderr重定向到此流。
 * @return StreamHandle_t 后台流的句柄，如果VTS未初始化则返回NULL。
 */
StreamHandle_t VTS_GetBackgroundStream(void);

#endif // MYRTOS_VTS_H
#endif
