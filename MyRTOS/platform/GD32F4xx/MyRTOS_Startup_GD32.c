#include "MyRTOS.h"
#include "MyRTOS_Config.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"

#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Shell.h"
#include "MyRTOS_Std.h"

#define MODULE_INIT_IF(condition, name, func) \
    do { \
        if (condition) { \
            func(); \
        } \
    } while(0)


/**
 * @brief ͳһ��ϵͳ��ʼ������
 *        �û��� main ��ֻ��Ҫ������һ��������
 *        ������������ļ���������ȷ������˳���ʼ������ģ�顣
 */
void MyRTOS_SystemInit(void) {
    MyRTOS_Init();
    MY_RTOS_MODULE_INIT_LIST();
}

#undef MODULE_INIT_IF
