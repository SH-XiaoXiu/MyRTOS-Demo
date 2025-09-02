/**
 * @file  MyRTOS_Stream_Def.h
 * @brief MyRTOS IO流服务 - 核心定义
 * @details 定义了流接口的底层结构和类型, 是IO、日志、Shell等模块的基础。
 */

#ifndef MYRTOS_STREAM_DEF_H
#define MYRTOS_STREAM_DEF_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_IO_ENABLE
#define MYRTOS_IO_ENABLE 0
#endif

#ifdef MYRTOS_SERVICE_IO_ENABLE

#include <stddef.h>
#include <stdint.h>

// 前置声明流的结构体
struct Stream_t;

/** @brief 流的句柄（Handle）类型，为指向流结构体的不透明指针。 */
typedef struct Stream_t *StreamHandle_t;

/** @brief 流的读函数指针（Function Pointer）类型规范。 */
typedef size_t (*StreamReadFn_t)(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

/** @brief 流的写函数指针（Function Pointer）类型规范。 */
typedef size_t (*StreamWriteFn_t)(StreamHandle_t stream, const void *buffer, size_t bytes_to_write,
                                  uint32_t block_ticks);

/** @brief 流的控制函数指针（Function Pointer）类型规范。 */
typedef int (*StreamControlFn_t)(StreamHandle_t stream, int command, void *arg);

/**
 * @brief 流接口（Interface）结构体定义。
 * @details 这是一个包含所有流操作函数指针的“虚函数表”，定义了一种流的行为。
 */
typedef struct {
    StreamReadFn_t read;
    StreamWriteFn_t write;
    StreamControlFn_t control;
} StreamInterface_t;

/**
 * @brief 流的基类（Base Class）结构体。
 * @details 所有具体的流对象都必须以此结构体作为其第一个成员，以支持统一的接口调用。
 */
typedef struct Stream_t {
    const StreamInterface_t *p_iface; // 指向实现了此流功能的接口表
    void *p_private_data; // 指向具体流的私有数据
} Stream_t;

#endif

#endif // MYRTOS_STREAM_DEF_H
