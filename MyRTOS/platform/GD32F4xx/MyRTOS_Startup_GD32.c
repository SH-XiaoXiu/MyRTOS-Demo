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
 * @brief 统一的系统初始化函数
 *        用户在 main 中只需要调用这一个函数。
 *        它会根据配置文件，按照正确的依赖顺序初始化所有模块。
 */
void MyRTOS_SystemInit(void) {
    MyRTOS_Init();
    MY_RTOS_MODULE_INIT_LIST();
}

#undef MODULE_INIT_IF
