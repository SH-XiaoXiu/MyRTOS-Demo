#ifndef MYRTOS_VTS_H
#define MYRTOS_VTS_H

#include "MyRTOS_Service_Config.h"

#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
#include <stdbool.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"

/**
 * @brief 模式枚举
 */
typedef enum {
    VTS_MODE_CANONICAL, // 行缓冲模式
    VTS_MODE_RAW        // 原始/裸模式
} VTS_TerminalMode_t;


/**
 * @brief VTS 初始化配置结构体
 */
typedef struct {
    StreamHandle_t physical_stream;
    StreamHandle_t root_input_stream;
    StreamHandle_t root_output_stream;
    TaskHandle_t   signal_receiver_task_handle; //非必填 一般给Shell
} VTS_Config_t;

/**
 * @brief 初始化并启动虚拟终端服务。
 * @details 此函数会创建VTS主任务。
 * @param config 指向VTS配置结构体的指针。
 * @return int 0 表示成功, -1 表示失败。
 */
int VTS_Init(const VTS_Config_t *config);

/**
 * @brief 设置当前的终端焦点。
 * @details 将物理终端的输入和输出切换到指定的流。
 * @param input_stream 新的焦点输入流。
 * @param output_stream 新的焦点输出流。
 * @return int 0 表示成功。
 */
int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream);

/**
 * @brief 将焦点重置回根（Shell）流。
 */
void VTS_ReturnToRootFocus(void);

/**
 * @brief 设置或取消“全局日志模式”。
 * @param enable true 启用, false 禁用。
 */
void VTS_SetLogAllMode(bool enable);

/**
 * @brief 获取VTS创建的后台流句柄。
 * @return StreamHandle_t 后台流的句柄。
 */
StreamHandle_t VTS_GetBackgroundStream(void);

/**
 * @brief 控制终端模式 (行缓冲/原始)。
 * @param mode 新的终端模式。
 * @return int 0 表示成功。
 */
int VTS_SetTerminalMode(VTS_TerminalMode_t mode);

/**
 * @brief 获取当前的终端模式。
 * @return VTS_TerminalMode_t 当前模式。
 */
VTS_TerminalMode_t VTS_GetTerminalMode(void);

#endif // MYRTOS_SERVICE_VTS_ENABLE
#endif // MYRTOS_VTS_H
