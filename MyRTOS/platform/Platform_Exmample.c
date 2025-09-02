// 平台移植层最小示例代码

#if 0

#include "MyRTOS.h"
#include "MyRTOS_Kernel_Private.h"
#include "MyRTOS_Port.h"

extern TaskHandle_t currentTask;


volatile UBaseType_t uxCriticalNesting = 0;

void MyRTOS_Port_EnterCritical(void) {
}

void MyRTOS_Port_ExitCritical(void) {
}

void MyRTOS_Port_Yield(void) {
}

void MyRTOS_Port_YieldFromISR(BaseType_t higherPriorityTaskWoken) {
}

StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
}


//启动调度
BaseType_t MyRTOS_Port_StartScheduler(void) {
}


void SVC_Handler(void) __attribute__((naked)) {
}

void PendSV_Handler(void) __attribute__((naked)) {
}


void SysTick_Handler(void) {
}

#endif
