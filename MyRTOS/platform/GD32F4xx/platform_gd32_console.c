#include "platform.h"
#if (PLATFORM_USE_CONSOLE == 1)

#include "MyRTOS_Port.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_usart.h"

// --- 内部数据和宏定义 ---
static char g_rx_buffer[PLATFORM_CONSOLE_RX_BUFFER_SIZE];
static volatile uint16_t g_rx_write_index = 0;
static volatile uint16_t g_rx_read_index = 0;
static SemaphoreHandle_t g_rx_semaphore = NULL;

// 根据配置选择USART外设
#if (PLATFORM_CONSOLE_USART_NUM == 0)
#define CONSOLE_USART USART0
#define CONSOLE_RCU_USART RCU_USART0
#define CONSOLE_RCU_GPIO RCU_GPIOA
#define CONSOLE_GPIO_PORT GPIOA
#define CONSOLE_TX_PIN GPIO_PIN_9
#define CONSOLE_RX_PIN GPIO_PIN_10
#define CONSOLE_GPIO_AF GPIO_AF_7
#define CONSOLE_IRQn USART0_IRQn
#define CONSOLE_IRQHandler USART0_IRQHandler
#elif (PLATFORM_CONSOLE_USART_NUM == 1)
// USART1 添加类似的宏定义 ...
#else
#error "Invalid PLATFORM_CONSOLE_USART_NUM selected in platform_config.h"
#endif

void Platform_fault_putchar(char c) {
    // 在HardFault中，不能无限等待。
    // 设置一个合理的超时计数，防止死锁。
    volatile uint32_t timeout = 0x000FFFFF;
    // 等待发送缓冲区为空，但带有超时
    while ((RESET == usart_flag_get(USART0, USART_FLAG_TBE)) && (timeout > 0)) {
        timeout--;
    }
    // 超时了，尝试强行发送
    usart_data_transmit(USART0, (uint8_t) c);
    // 如果需要确保发送完成，可以再加一个带超时的等待
    timeout = 0x000FFFFF;
    while ((RESET == usart_flag_get(USART0, USART_FLAG_TC)) && (timeout > 0)) {
        timeout--;
    }
}

static size_t console_stream_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    (void) stream;
    char *p_buf = (char *) buffer;
    size_t bytes_read;
    for (bytes_read = 0; bytes_read < bytes_to_read; bytes_read++) {
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

static size_t console_stream_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write,
                                   uint32_t block_ticks) {
    (void) stream;
    (void) block_ticks;
    const char *p_buf = (const char *) buffer;
    for (size_t i = 0; i < bytes_to_write; i++) {
        if (p_buf[i] == '\n') {
            Platform_fault_putchar('\r');
        }
        Platform_fault_putchar(p_buf[i]);
    }
    return bytes_to_write;
}

static const StreamInterface_t g_console_stream_interface = {
    .read = console_stream_read,
    .write = console_stream_write,
    .control = NULL,
};
static Stream_t g_console_stream_instance;

// --- 公共API ---
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
    if (g_rx_semaphore == NULL) {
        g_rx_semaphore = Semaphore_Create(PLATFORM_CONSOLE_RX_BUFFER_SIZE, 0);
    }
    nvic_irq_enable(CONSOLE_IRQn, PLATFORM_CONSOLE_IRQ_PRIORITY, 0);
    usart_interrupt_enable(CONSOLE_USART, USART_INT_RBNE);
    usart_enable(CONSOLE_USART);
    g_console_stream_instance.p_iface = &g_console_stream_interface;
    g_console_stream_instance.p_private_data = NULL;
}

void Platform_Console_OSInit(void) { g_rx_semaphore = Semaphore_Create(PLATFORM_CONSOLE_RX_BUFFER_SIZE, 0); }

StreamHandle_t Platform_Console_GetStream(void) { return &g_console_stream_instance; }

// --- 中断处理函数 ---
void CONSOLE_IRQHandler(void) {
    if (RESET != usart_interrupt_flag_get(CONSOLE_USART, USART_INT_FLAG_RBNE)) {
        char received_char = (char) usart_data_receive(CONSOLE_USART);
        uint16_t next_write_index = (g_rx_write_index + 1) % PLATFORM_CONSOLE_RX_BUFFER_SIZE;
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
