#ifndef MYRTOS_STANDARD_H
#define MYRTOS_STANDARD_H

#include "MyRTOS_Config.h"

// ======================================================================
//                       ��׼��ݺ� (Standard Convenience Macros)
// ======================================================================
// Ϊ�˷���Ӧ�ó��򿪷����ṩһ�����Ʊ�׼C��ĺ�

#if (MY_RTOS_USE_STDIO == 1)
#define printf(...)    MyRTOS_printf(__VA_ARGS__)
//��ӡ�ַ������Զ�����
#define puts(s)        MyRTOS_printf("%s\n", s)
//��ӡ�����ַ�
#define putchar(c)     MyRTOS_printf("%c", c)
#else
#define printf(...)    ((void)0)
#define puts(s)        ((void)0)
#define putchar(c)     ((void)0)
#endif


// ======================================================================
//                       ��׼����ͷ�ļ�����
// ======================================================================
// ���ͷ�ļ�ּ���ṩMyRTOS��úͱ�׼�ķ���ӿڡ�

#if (MY_RTOS_USE_STDIO == 1)
#include "MyRTOS_IO.h"
#endif

#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif

// ======================================================================
//                       ��׼������ (Standard Return Codes)
// ======================================================================
// ����һ��ͳһ�ķ����룬��������API
typedef enum {
    MYRTOS_OK = 0, // �����ɹ�
    MYRTOS_ERROR = -1, // ͨ�ô���
    MYRTOS_TIMEOUT = -2, // ������ʱ
    MYRTOS_INVALID_ARG = -3, // ��Ч����
    MYRTOS_NO_MEM = -4, // �ڴ治��
} MyRTOS_Status_t;


// ======================================================================
//                       ��׼����ӿڣ�ϵͳ������
// ======================================================================
#if (MY_RTOS_USE_MONITOR == 1)

/**
 * @brief ����ϵͳ����������
 *        ������������һ����̨���񣬸�����������Եؽ�ϵͳ״̬
 *        ��ӡ����׼�����������������У�����Ч����
 * @return MYRTOS_OK �ɹ�, MYRTOS_ERROR ʧ�ܡ�
 */
MyRTOS_Status_t MyRTOS_Monitor_Start(void);

/**
 * @brief ֹͣϵͳ����������
 *        �����ٺ�̨�ļ������������δ���У�����Ч����
 * @return MYRTOS_OK �ɹ���
 */
MyRTOS_Status_t MyRTOS_Monitor_Stop(void);

/**
 * @brief ��ѯ�����������Ƿ��������С�
 * @return 1 ��������, 0 ��ֹͣ��
 */
int MyRTOS_Monitor_IsRunning(void);

#endif // MY_RTOS_USE_MONITOR


// ======================================================================
//                       ����δ�����ܵı�׼���.
// ======================================================================
/*
#if (MY_RTOS_USE_FILESYSTEM == 1)
    // �ļ�ϵͳ��صı�׼�ӿڶ���...
#endif
*/


#endif // MY_RTOS_STANDARD_H
