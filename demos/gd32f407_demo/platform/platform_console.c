//
// GD32F407 Demo Platform Console Driver
//
// GD32F407 USART 控制台驱动
//

#include "platform.h"
#if (PLATFORM_USE_CONSOLE == 1)

#include "MyRTOS_Port.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_usart.h"

// 根据平台配置选择USART外设及相关资源
#if (PLATFORM_CONSOLE_USART_NUM == 0)
#define CONSOLE_USART       USART0
#define CONSOLE_RCU_USART   RCU_USART0
#define CONSOLE_RCU_GPIO    RCU_GPIOA
#define CONSOLE_GPIO_PORT   GPIOA
#define CONSOLE_TX_PIN      GPIO_PIN_9
#define CONSOLE_RX_PIN      GPIO_PIN_10
#define CONSOLE_GPIO_AF     GPIO_AF_7
#define CONSOLE_IRQn        USART0_IRQn
#define CONSOLE_IRQHandler  USART0_IRQHandler
#elif (PLATFORM_CONSOLE_USART_NUM == 1)
// 此处为 USART1 添加类似的宏定义
#else
#error "Invalid PLATFORM_CONSOLE_USART_NUM selected in platform_config.h"
#endif

// ============================================================================
//                           模块全局变量
// ============================================================================

// 接收环形缓冲区
static char g_rx_buffer[PLATFORM_CONSOLE_RX_BUFFER_SIZE];
static volatile uint16_t g_rx_write_index = 0;
static volatile uint16_t g_rx_read_index = 0;

// 用于同步接收中断和读取任务的信号量
static SemaphoreHandle_t g_rx_semaphore = NULL;

// 控制台流接口实例
static const StreamInterface_t g_console_stream_interface;
static Stream_t g_console_stream_instance;

// ============================================================================
//                           私有函数声明
// ============================================================================
static size_t console_stream_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);
static size_t console_stream_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks);

// ============================================================================
//                           公共函数实现
// ============================================================================

// 在 HardFault 中使用的非阻塞字符发送函数
void Platform_fault_putchar(char c) {
    volatile uint32_t timeout = 0x000FFFFF;
    // 等待发送缓冲区为空, 带超时
    while ((RESET == usart_flag_get(CONSOLE_USART, USART_FLAG_TBE)) && (timeout > 0)) {
        timeout--;
    }
    usart_data_transmit(CONSOLE_USART, (uint8_t) c);
}

// 阻塞式地发送一个字符
static void Platform_putchar(char c) {
    // 等待发送数据寄存器为空
    while (RESET == usart_flag_get(CONSOLE_USART, USART_FLAG_TBE)) {
    }
    // 写入数据进行发送
    usart_data_transmit(CONSOLE_USART, (uint8_t) c);
    // 等待发送完成
    while (RESET == usart_flag_get(CONSOLE_USART, USART_FLAG_TC)) {
    }
}

// 初始化控制台的硬件部分
void Platform_Console_HwInit(void) {
    rcu_periph_clock_enable(CONSOLE_RCU_GPIO);
    rcu_periph_clock_enable(CONSOLE_RCU_USART);

    gpio_af_set(CONSOLE_GPIO_PORT, CONSOLE_GPIO_AF, CONSOLE_TX_PIN);
    gpio_af_set(CONSOLE_GPIO_PORT, CONSOLE_GPIO_AF, CONSOLE_RX_PIN);

    gpio_mode_set(CONSOLE_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, CONSOLE_TX_PIN | CONSOLE_RX_PIN);
    gpio_output_options_set(CONSOLE_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CONSOLE_TX_PIN | CONSOLE_RX_PIN);

    usart_deinit(CONSOLE_USART);
    usart_baudrate_set(CONSOLE_USART, PLATFORM_CONSOLE_BAUDRATE);
    usart_receive_config(CONSOLE_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(CONSOLE_USART, USART_TRANSMIT_ENABLE);

    nvic_irq_enable(CONSOLE_IRQn, PLATFORM_CONSOLE_IRQ_PRIORITY, 0);
    usart_interrupt_enable(CONSOLE_USART, USART_INT_RBNE);

    usart_enable(CONSOLE_USART);

    g_console_stream_instance.p_iface = &g_console_stream_interface;
    g_console_stream_instance.p_private_data = NULL;
}

// 初始化控制台的操作系统相关部分
void Platform_Console_OSInit(void) {
    if (g_rx_semaphore == NULL) {
        g_rx_semaphore = Semaphore_Create(PLATFORM_CONSOLE_RX_BUFFER_SIZE, 0);
    }
}

// 获取控制台的流句柄
StreamHandle_t Platform_Console_GetStream(void) {
    return &g_console_stream_instance;
}

// ============================================================================
//                           流接口实现
// ============================================================================

// 控制台流的读操作实现
static size_t console_stream_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    (void) stream;
    char *p_buf = (char *) buffer;
    size_t bytes_read;
    for (bytes_read = 0; bytes_read < bytes_to_read; bytes_read++) {
        // 等待信号量, 表示缓冲区中有数据
        if (Semaphore_Take(g_rx_semaphore, block_ticks) != 1) {
            break;
        }
        MyRTOS_Port_EnterCritical();
        p_buf[bytes_read] = g_rx_buffer[g_rx_read_index];
        g_rx_read_index = (g_rx_read_index + 1) % PLATFORM_CONSOLE_RX_BUFFER_SIZE;
        MyRTOS_Port_ExitCritical();
    }
    return bytes_read;
}

// 控制台流的写操作实现
static size_t console_stream_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    (void) stream;
    (void) block_ticks;
    const char *p_buf = (const char *) buffer;
    for (size_t i = 0; i < bytes_to_write; i++) {
        // 自动将换行符 '\n' 转换为 '\r\n'
        if (p_buf[i] == '\n') {
            Platform_putchar('\r');
        }
        Platform_putchar(p_buf[i]);
    }
    return bytes_to_write;
}

// 流接口定义
static const StreamInterface_t g_console_stream_interface = {
    .read = console_stream_read,
    .write = console_stream_write,
    .control = NULL,
};

// ============================================================================
//                           中断服务例程
// ============================================================================

// USART 中断服务例程
void CONSOLE_IRQHandler(void) {
    if (RESET != usart_interrupt_flag_get(CONSOLE_USART, USART_INT_FLAG_RBNE)) {
        char received_char = (char) usart_data_receive(CONSOLE_USART);
        uint16_t next_write_index = (g_rx_write_index + 1) % PLATFORM_CONSOLE_RX_BUFFER_SIZE;

        // 检查环形缓冲区是否已满
        if (next_write_index != g_rx_read_index) {
            g_rx_buffer[g_rx_write_index] = received_char;
            g_rx_write_index = next_write_index;

            int higher_priority_task_woken = 0;
            Semaphore_GiveFromISR(g_rx_semaphore, &higher_priority_task_woken);
            MyRTOS_Port_YieldFromISR(higher_priority_task_woken);
        }
    }
}

#endif // PLATFORM_USE_CONSOLE
