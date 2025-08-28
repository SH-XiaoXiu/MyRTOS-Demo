//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: GD32F4xx Series
//

#include "MyRTOS_Driver_Timer.h"
#include "gd32f4xx.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_timer.h"

// 内部设备描述结构体
typedef struct {
    uint32_t periph_base; // e.g., TIMER1
    rcu_periph_enum rcu_clock;
    uint32_t configured_freq; // 期望的配置频率
} TimerDevice_t;

// 根据配置，静态实例化设备列表
#undef USE_TIMER
#define USE_TIMER(id, dev, param) \
    [TIMER_ID_##id] = { \
        .periph_base = dev, \
        .rcu_clock = RCU_##dev, \
        .configured_freq = 0, \
    },
static TimerDevice_t timer_devices[TIMER_ID_MAX] = {
    MY_RTOS_TIMER_DEVICE_LIST
};

/**
 * @brief 初始化在 MyRTOS_Config.h 中定义的所有定时器
 */
void MyRTOS_Timer_Init(void) {
    timer_parameter_struct timer_init_parameter;
    for (int id = 0; id < TIMER_ID_MAX; id++) {
        rcu_periph_clock_enable(timer_devices[id].rcu_clock);
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        if (id == MY_RTOS_STATS_TIMER_ID) {
            timer_deinit(timer_devices[id].periph_base);
            uint32_t prescaler = (SystemCoreClock / MY_RTOS_STATS_TIMER_FREQ_HZ) - 1;
            timer_init_parameter.prescaler = prescaler;
            timer_init_parameter.alignedmode = TIMER_COUNTER_EDGE;
            timer_init_parameter.counterdirection = TIMER_COUNTER_UP;
            timer_init_parameter.period = 0xFFFFFFFF;
            timer_init_parameter.clockdivision = TIMER_CKDIV_DIV1;
            timer_init_parameter.repetitioncounter = 0;
            timer_init(timer_devices[id].periph_base, &timer_init_parameter);
            timer_devices[id].configured_freq = MY_RTOS_STATS_TIMER_FREQ_HZ;
            timer_enable(timer_devices[id].periph_base);
        }
#endif
        // 此处可以为其他定时器添加默认初始化
        // if (id == TIMER_ID_USER_APP_TIMER) { ... }
    }
}

/**
 * @brief 根据逻辑ID获取指定的定时器句柄
 */
TimerHandle_dev_t MyRTOS_Timer_GetHandle(TimerID_t id) {
    if (id < TIMER_ID_MAX) {
        return &timer_devices[id];
    }
    return NULL;
}

/**
 * @brief 启动一个定时器
 */
int MyRTOS_Timer_Start(TimerHandle_dev_t handle) {
    if (handle == NULL) return -1;
    TimerDevice_t *device = (TimerDevice_t *) handle;
    timer_enable(device->periph_base);
    return 0;
}

/**
 * @brief 停止一个定时器
 */
int MyRTOS_Timer_Stop(TimerHandle_dev_t handle) {
    if (handle == NULL) return -1;
    TimerDevice_t *device = handle;
    timer_disable(device->periph_base);
    return 0;
}

/**
 * @brief 获取定时器的当前计数值
 */
uint32_t MyRTOS_Timer_GetCount(TimerHandle_dev_t handle) {
    if (handle == NULL) return 0;
    TimerDevice_t *device = (TimerDevice_t *) handle;
    return timer_counter_read(device->periph_base);
}

/**
 * @brief 设置定时器的周期 (自动重装载值)
 */
int MyRTOS_Timer_SetPeriod(TimerHandle_dev_t handle, uint32_t period) {
    if (handle == NULL) return -1;
    TimerDevice_t *device = (TimerDevice_t *) handle;
    timer_autoreload_value_config(device->periph_base, period);
    return 0;
}

/**
 * @brief 获取定时器的计数频率 (Hz)
 */
uint32_t MyRTOS_Timer_GetFreq(TimerHandle_dev_t handle) {
    if (handle == NULL) return 0;
    TimerDevice_t *device = (TimerDevice_t *) handle;
    return device->configured_freq;
}
