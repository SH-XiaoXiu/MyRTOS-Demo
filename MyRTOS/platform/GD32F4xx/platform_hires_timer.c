#include "platform.h"
#if (PLATFORM_USE_HIRES_TIMER == 1)

#include "gd32f4xx_timer.h"
#include "gd32f4xx_rcu.h"

// 根据配置选择TIMER外设
#if (PLATFORM_HIRES_TIMER_NUM == 1)
#define HIRES_TIMER         TIMER1
#define HIRES_TIMER_RCU     RCU_TIMER1
#elif (PLATFORM_HIRES_TIMER_NUM == 2)
#define HIRES_TIMER         TIMER2
#define HIRES_TIMER_RCU     RCU_TIMER2
// ... 为其他定时器添加宏定义 ...
#else
#error "Invalid PLATFORM_HIRES_TIMER_NUM selected in platform_config.h"
#endif

void Platform_HiresTimer_Init(void) {
    timer_parameter_struct timer_initpara;
    rcu_periph_clock_enable(HIRES_TIMER_RCU);
    timer_deinit(HIRES_TIMER);
    timer_struct_para_init(&timer_initpara);
    // 配置预分频器，使定时器频率达到配置值
    timer_initpara.prescaler = (uint16_t) (SystemCoreClock / PLATFORM_HIRES_TIMER_FREQ_HZ - 1);
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = 0xFFFFFFFF; // 32位自由运行
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(HIRES_TIMER, &timer_initpara);
    timer_enable(HIRES_TIMER);
}

uint32_t Platform_Timer_GetHiresValue(void) {
    return timer_counter_read(HIRES_TIMER);
}
#endif // PLATFORM_USE_HIRES_TIMER
