/**
 * @file myrtos_extension.c
 * @brief MyRTOS 内核扩展机制模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 私有变量
 *===========================================================================*/

// 内核扩展回调函数数组
static KernelExtensionCallback_t g_extensions[MAX_KERNEL_EXTENSIONS] = {NULL};
// 已注册的内核扩展数量
static uint8_t g_extension_count = 0;

/*===========================================================================*
 * 内部函数实现
 *===========================================================================*/

/**
 * @brief 向所有已注册的内核扩展广播一个事件
 * @param pEventData 要广播的事件数据
 */
void broadcast_event(const KernelEventData_t *pEventData) {
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i]) {
            g_extensions[i](pEventData);
        }
    }
}

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

/**
 * @brief 注册一个内核扩展回调函数
 * @note  内核扩展可用于调试、跟踪或实现自定义功能，它会在特定内核事件发生时被调用。
 * @param callback 要注册的回调函数指针
 * @return 成功返回0，失败返回-1
 */
int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback) {
    MyRTOS_Port_EnterCritical();
    // 检查扩展槽是否已满或回调函数是否为空
    if (g_extension_count >= MAX_KERNEL_EXTENSIONS || callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    // 避免重复注册
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
    // 添加新的回调
    g_extensions[g_extension_count++] = callback;
    MyRTOS_Port_ExitCritical();
    return 0;
}

/**
 * @brief 注销一个内核扩展回调函数
 * @param callback 要注销的回调函数指针
 * @return 成功返回0，失败（未找到该回调）返回-1
 */
int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback) {
    int found = 0;
    MyRTOS_Port_EnterCritical();
    if (callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            // 将最后一个元素移到当前位置，然后缩减计数
            g_extensions[i] = g_extensions[g_extension_count - 1];
            g_extensions[g_extension_count - 1] = NULL;
            g_extension_count--;
            found = 1;
            break;
        }
    }
    MyRTOS_Port_ExitCritical();
    return found ? 0 : -1;
}
