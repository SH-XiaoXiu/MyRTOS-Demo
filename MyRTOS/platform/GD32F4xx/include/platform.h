//
// Created by XiaoXiu on 8/31/2025.
//

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include "platform_config.h"
#include "gd32f4xx.h"

// ���� MyRTOS �������ͣ��Ա㹳�Ӻ�������ʹ��
#include "MyRTOS.h"
#include "MyRTOS_Shell.h"

// =========================================================================
//                         ����ƽ̨��ʼ������
// =========================================================================
/**
 * @brief ��ʼ������ƽ̨���RTOS���ķ���
 *        �����û��� main ��������Ҫ���õĵ�һ��������
 */
void Platform_Init(void);


/**
 * @brief ����ƽ̨��RTOS��������
 *        ������������ Platform_CreateTasks_Hook ������Ӧ������
 *        Ȼ������RTOS���������˺����������ء�
 */
void Platform_StartScheduler(void);


// =========================================================================
//                            ƽ̨�����ȡ�ӿ�
// =========================================================================
#if (PLATFORM_USE_CONSOLE == 1)
/**
 * @brief ��ȡ�ѳ�ʼ���Ŀ���̨�������
 * @return StreamHandle_t ָ�����̨���ľ������δʹ���򷵻�NULL��
 */
StreamHandle_t Platform_Console_GetStream(void);
#endif

#if (PLATFORM_USE_HIRES_TIMER == 1)
/**
 * @brief ��ȡ�߾��ȶ�ʱ���ĵ�ǰ����ֵ��
 * @return uint32_t 32λ����ֵ��
 */
uint32_t Platform_Timer_GetHiresValue(void);
#endif


#if MYRTOS_SERVICE_SHELL_ENABLE ==1
struct ShellCommand_t;
// =========================================================================
//                            Shell ����ע��
// =========================================================================
/**
 * @brief ��ƽ̨ע��һ�������û��Զ����Shell���
 * @details �û�Ӧ���� main ������ Platform_AppSetup_Hook �����е��ô˺�����
 *          ƽ̨����ռ�����ע�������������ͳһ��ʼ��Shell����
 *          ������ help ����ʱ������ע�������ᱻ��ʾ������
 * @param commands      [in] ָ�� ShellCommand_t ���������ָ�롣
 * @param command_count [in] �����������������
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾʧ�� (���磬�����б�����)��
 */
int Platform_RegisterShellCommands(const struct ShellCommand_t *commands, size_t command_count);
#endif


// =========================================================================
//                      �û��Զ��平�Ӻ��� (Weak Hooks)
//               �û��������Լ��Ĵ���������ʵ����Щ����
// =========================================================================

/**
 * @brief ��ƽ̨����ʱ�ӳ�ʼ��֮�󣬵������������ʼ��֮ǰ���á�
 *        �ʺ��������õ�Դ�����Ÿ��õ��������á�
 */
void Platform_EarlyInit_Hook(void);

/**
 * @brief ������ƽ̨��������USART, TIMER����ʼ��֮����á�
 *        ���Ƽ��������û���ʼ���Լ��ض�Ӳ�����紫������LED���������ĵط���
 */
void Platform_BSP_Init_Hook(void);

/**
 * @brief ��RTOS������Log, Shell����ʼ��֮�󣬵��ڴ����κ�Ӧ������֮ǰ���á�
 *        ���Ƽ��������û�ע���Զ���Shell����ĵط���
 * @param shell_h Shell����ľ�������Shell����ʹ�ܣ���
 */
void Platform_AppSetup_Hook(ShellHandle_t shell_h);

/**
 * @brief �ڵ���������֮ǰ�����ڴ�������Ӧ�ó�������
 *        ���Ƽ��������û������Լ�ҵ���߼�����ĵط���
 */
void Platform_CreateTasks_Hook(void);

/**
 * @brief RTOS ���������ʵ�֡�
 *        �û�������д�˺������ڿ���ʱִ�е͹��Ĳ�����������̨����
 */
void Platform_IdleTask_Hook(void *pv);

/**
 * @brief Ӳ������ (HardFault) �Ĵ�������
 *        Ĭ��ʵ�ֻ��ӡ������Ϣ������ϵͳ���û�������д��ʵ���Զ�����Ϊ�����¼��־����������
 * @param pulFaultStackAddress ָ����Ϸ���ʱ��ջ��ָ�롣
 */
void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress);

/**
 * @brief �����ջ����Ĵ�������
 *        Ĭ��ʵ�ֻ��ӡ������Ϣ������ϵͳ��
 * @param pxTask �����������������
 */
void Platform_StackOverflow_Hook(TaskHandle_t pxTask);


/**
 * @brief �ں��ڴ����ʧ�ܵĴ�������
 * @param wantedSize ���Է��䵫ʧ�ܵ��ֽ�����
 */
void Platform_MallocFailed_Hook(size_t wantedSize);

#endif // PLATFORM_H
