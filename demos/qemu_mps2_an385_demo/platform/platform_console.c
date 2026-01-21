/**
 * @brief QEMU MPS2-AN385 控制台驱动 (UART0)
 *        支持 RX 中断，实现低延迟输入
 */

#include "platform.h"
#include "CMSDK_CM3.h"

#if PLATFORM_USE_CONSOLE == 1 && MYRTOS_SERVICE_IO_ENABLE == 1

#include "MyRTOS_Port.h"

// ============================================================================
//                           模块全局变量
// ============================================================================

static SemaphoreHandle_t s_tx_semaphore = NULL;
static SemaphoreHandle_t s_rx_semaphore = NULL;

// 控制台流接口实例
static const StreamInterface_t g_console_stream_interface;
static Stream_t g_console_stream_instance;

// ============================================================================
//                           底层 UART 函数
// ============================================================================

/**
 * @brief 发送单个字符（阻塞）
 */
static void uart_putchar(char c) {
    // 等待 TX 缓冲区空
    while (CMSDK_UART0->STATE & CMSDK_UART_STATE_TXFULL_Msk);
    CMSDK_UART0->DATA = (uint32_t)c;
}

/**
 * @brief 在 HardFault 中使用的非阻塞字符发送函数
 */
void Platform_fault_putchar(char c) {
    volatile uint32_t timeout = 0x000FFFFF;
    while ((CMSDK_UART0->STATE & CMSDK_UART_STATE_TXFULL_Msk) && (timeout > 0)) {
        timeout--;
    }
    CMSDK_UART0->DATA = (uint32_t)c;
}

/**
 * @brief 接收单个字符（非阻塞）
 * @return 字符，-1 表示无数据
 */
static int uart_getchar_nonblock(void) {
    if (CMSDK_UART0->STATE & CMSDK_UART_STATE_RXFULL_Msk) {
        return (int)(CMSDK_UART0->DATA & 0xFF);
    }
    return -1;
}

// ============================================================================
//                           中断处理
// ============================================================================

/**
 * @brief UART0 中断处理函数
 */
void UART0_Handler(void) {
    // 读取中断状态
    uint32_t status = CMSDK_UART0->INTSTATUS;

    // 检查是否为 RX 中断
    if (status & CMSDK_UART_INTSTATUS_RX_Msk) {
        // 写入 INTSTATUS 来清除中断（CMSDK UART 需要写入才能清除）
        CMSDK_UART0->INTSTATUS = CMSDK_UART_INTSTATUS_RX_Msk;

        // 通知接收信号量
        if (s_rx_semaphore) {
            int woken = 0;
            Semaphore_GiveFromISR(s_rx_semaphore, &woken);
            MyRTOS_Port_YieldFromISR(woken);
        }
    }
}

// ============================================================================
//                           流接口实现
// ============================================================================

static size_t console_stream_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    (void)stream;
    (void)block_ticks;
    const char *p = (const char *)buffer;

    if (s_tx_semaphore) {
        Semaphore_Take(s_tx_semaphore, MYRTOS_MAX_DELAY);
    }

    for (size_t i = 0; i < bytes_to_write; i++) {
        if (p[i] == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(p[i]);
    }

    if (s_tx_semaphore) {
        Semaphore_Give(s_tx_semaphore);
    }

    return bytes_to_write;
}

static size_t console_stream_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    (void)stream;
    char *p = (char *)buffer;
    size_t count = 0;

    while (count < bytes_to_read) {
        // 尝试读取
        int c = uart_getchar_nonblock();
        if (c >= 0) {
            p[count++] = (char)c;
        } else {
            // 没有数据
            if (count > 0) {
                // 已经读到一些数据，返回
                break;
            }
            // 等待 RX 中断信号量
            if (s_rx_semaphore) {
                if (Semaphore_Take(s_rx_semaphore, block_ticks) != 1) {
                    // 超时
                    break;
                }
            } else {
                // 没有信号量，用轮询
                Task_Delay(1);
            }
        }
    }

    return count;
}

// 流接口定义
static const StreamInterface_t g_console_stream_interface = {
    .read = console_stream_read,
    .write = console_stream_write,
    .control = NULL,
};

// ============================================================================
//                           公共接口
// ============================================================================

void Platform_Console_HwInit(void) {
    // 配置 UART0
    CMSDK_UART0->BAUDDIV = SystemCoreClock / 115200;
    // 启用 TX、RX 和 RX 中断
    CMSDK_UART0->CTRL = CMSDK_UART_CTRL_TXEN_Msk | CMSDK_UART_CTRL_RXEN_Msk | CMSDK_UART_CTRL_RXIRQEN_Msk;

    // 配置 NVIC
    NVIC_SetPriority(UART0_IRQn, 5);
    NVIC_EnableIRQ(UART0_IRQn);

    // 初始化控制台流实例
    g_console_stream_instance.p_iface = &g_console_stream_interface;
    g_console_stream_instance.p_private_data = NULL;
}

void Platform_Console_OSInit(void) {
    // 创建发送信号量（互斥）
    s_tx_semaphore = Semaphore_Create(1, 1);
    // 创建接收信号量
    s_rx_semaphore = Semaphore_Create(16, 0);
}

StreamHandle_t Platform_Console_GetStream(void) {
    return &g_console_stream_instance;
}

#endif /* PLATFORM_USE_CONSOLE && MYRTOS_SERVICE_IO_ENABLE */
