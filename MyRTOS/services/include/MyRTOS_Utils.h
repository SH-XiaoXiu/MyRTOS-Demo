//
// Created by XiaoXiu on 9/5/2025.
//

/**
 * @file  MyRTOS_Utils.h
 * @brief MyRTOS 内部通用工具函数 - 公共接口
 */
#ifndef MYRTOS_UTILS_H
#define MYRTOS_UTILS_H

#include <stdarg.h>
#include <stddef.h>
/**
 * @brief   一个可替换的、安全的格式化字符串核心函数
 * @details 此函数封装了底层的字符串格式化库
 *          该实现保证了无论何种情况，输出缓冲区总是以'\0'结尾。
 * @param   buffer      用于存放结果的字符缓冲区。
 * @param   size         缓冲区的总大小。
 * @param   format       格式化控制字符串。
 * @param   args         可变参数列表 (va_list)
 * @return  int         返回若缓冲区足够大时，本应写入的字符数 (不包括结尾的'\0')
 *                      如果返回值大于或等于 'size'，表示输出被截断
 *                      返回负值表示发生编码错误
 */
int MyRTOS_FormatV(char *buffer, size_t size, const char *format, va_list args);

#endif // MYRTOS_UTILS_H
