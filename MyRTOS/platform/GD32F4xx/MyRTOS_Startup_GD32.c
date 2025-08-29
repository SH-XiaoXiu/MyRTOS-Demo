//
// Created by XiaoXiu on 8/30/2025.
//
// Platform: GD32F4xx
//

#include "MyRTOS.h"
#include "MyRTOS_Config.h"
#include "MyRTOS_Console.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"
#include "MyRTOS_Terminal.h"

#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif

/**
 * @brief ͳһ��ϵͳ��ʼ������
 *        �û��� main ��ֻ��Ҫ������һ������
 */
void MyRTOS_SystemInit(void) {
    //��ײ�İ弶֧�ֳ�ʼ�� (����: ���Դ���)
    MyRTOS_Platform_Init();
    //Ӳ��������ʼ�� (����: ��ʱ��)
#if (defined(MY_RTOS_TIMER_DEVICE_LIST))
    MyRTOS_Timer_Init();
#endif
    //�ں˳�ʼ��
    MyRTOS_Init();
    //������ʼ��
#if (MY_RTOS_USE_CONSOLE == 1)
    MyRTOS_Console_Init();
#endif
#if (MY_RTOS_USE_LOG == 1)
    MyRTOS_Log_Init();
#endif
#if (MY_RTOS_USE_TERMINAL == 1)
    MyRTOS_Terminal_Init();
#endif
}
