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

#include "MyRTOS_Stream_Def.h"

/**
 * @brief VTS创建的虚拟流句柄集合。
 */
typedef struct {
    /** @brief 主要交互通道的输入流 (数据流向: VTS -> 交互任务)。*/
    StreamHandle_t primary_input_stream;

    /** @brief 主要交互通道的输出流 (数据流向: 交互任务 -> VTS)。*/
    StreamHandle_t primary_output_stream;

    /** @brief 用于汇集所有其他任务输出的后台流 (数据流向: 后台任务 -> VTS)。*/
    StreamHandle_t background_stream;
} VTS_Handles_t;

/**
 * @brief 初始化并启动虚拟终端服务。
 * @param physical_stream [in] 指向物理终端流的句柄。
 * @param handles         [out] 用于接收VTS创建的虚拟流句柄。
 * @return int 0 表示成功, -1 表示失败。
 */
int VTS_Init(StreamHandle_t physical_stream, VTS_Handles_t *handles);

/**
 * @brief 设置当前拥有终端焦点的输出流。
 * @details 只有被设置为焦点的流的输出，才会被VTS路由到物理终端。
 *          所有其他流的输出将被丢弃。
 * @param stream [in] 要设置为焦点的流句柄。可以是 VTS 创建的任何输出流，
 *                    或者是 NULL 来取消所有焦点（静默模式）。
 * @return int 0 表示成功, -1 表示流未被VTS管理。
 */
int VTS_SetFocus(StreamHandle_t stream);

#endif // MYRTOS_SERVICE_VTS_ENABLE

#endif // MYRTOS_VTS_H