#include "MyRTOS.h"
#include "MyRTOS_Config.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"

// ����������Ҫ��ʼ���ķ���ͷ�ļ�
#if (MY_RTOS_USE_STDIO == 1)
#include "MyRTOS_IO.h"
#endif
#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif
#if (MY_RTOS_USE_SHELL == 1)
#include "MyRTOS_Shell.h"
#endif


/**
 * @brief ͳһ��ϵͳ��ʼ������
 *        �û��� main ��ֻ��Ҫ������һ��������
 *        ������������ļ���������ȷ������˳���ʼ������ģ�顣
 */
void MyRTOS_SystemInit(void) {
    MyRTOS_Platform_Init();
#if (defined(MY_RTOS_TIMER_DEVICE_LIST))
    MyRTOS_Timer_Init();
#endif
#if (MY_RTOS_USE_STDIO == 1)
    MyRTOS_StdIO_Init();
#endif
#if (MY_RTOS_USE_LOG == 1)
    MyRTOS_Log_Init();
#endif
#if (MY_RTOS_USE_SHELL == 1)
    MyRTOS_Shell_Init();
#endif
}