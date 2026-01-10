#include "MyRTOS_VTS.h"

#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "MyRTOS.h"


// VTS内部实例结构体
typedef struct {
    VTS_Config_t config;
    MutexHandle_t lock;
    TaskHandle_t vts_task_handle;
    StreamHandle_t background_stream;
    volatile VTS_TerminalMode_t terminal_mode;
    volatile StreamHandle_t focused_input_stream;
    volatile StreamHandle_t focused_output_stream;
    char line_buffer[VTS_LINE_BUFFER_SIZE];
    int buffer_len;
    int cursor_pos;

    enum { ANSI_STATE_NORMAL, ANSI_STATE_ESC, ANSI_STATE_BRACKET } ansi_state;
} VTS_Instance_t;

static VTS_Instance_t *g_vts = NULL;

static void vts_write_physical(const char *data, size_t len) {
    Stream_Write(g_vts->config.physical_stream, data, len, MYRTOS_MAX_DELAY);
}

static void handle_canonical_input(char ch) {
    if (isprint((unsigned char) ch)) {
        if (g_vts->buffer_len < VTS_LINE_BUFFER_SIZE - 1) {
            memmove(&g_vts->line_buffer[g_vts->cursor_pos + 1], &g_vts->line_buffer[g_vts->cursor_pos],
                    g_vts->buffer_len - g_vts->cursor_pos);
            g_vts->line_buffer[g_vts->cursor_pos] = ch;
            g_vts->buffer_len++;
            g_vts->cursor_pos++;
            vts_write_physical(&g_vts->line_buffer[g_vts->cursor_pos - 1], g_vts->buffer_len - (g_vts->cursor_pos - 1));
            for (int i = 0; i < g_vts->buffer_len - g_vts->cursor_pos; i++) vts_write_physical("\x1b[D", 3);
        }
    } else if (ch == '\r' || ch == '\n') {
        vts_write_physical("\r\n", 2);
        Stream_Write(g_vts->focused_input_stream, g_vts->line_buffer, g_vts->buffer_len, MYRTOS_MAX_DELAY);
        Stream_Write(g_vts->focused_input_stream, "\n", 1, MYRTOS_MAX_DELAY);
        memset(g_vts->line_buffer, 0, VTS_LINE_BUFFER_SIZE);
        g_vts->buffer_len = 0;
        g_vts->cursor_pos = 0;
    } else if (ch == '\b' || ch == 127) {
        if (g_vts->cursor_pos > 0) {
            memmove(&g_vts->line_buffer[g_vts->cursor_pos - 1], &g_vts->line_buffer[g_vts->cursor_pos],
                    g_vts->buffer_len - g_vts->cursor_pos);
            g_vts->buffer_len--;
            g_vts->cursor_pos--;
            g_vts->line_buffer[g_vts->buffer_len] = '\0';
            vts_write_physical("\b", 1);
            vts_write_physical(&g_vts->line_buffer[g_vts->cursor_pos], g_vts->buffer_len - g_vts->cursor_pos);
            vts_write_physical(" ", 1);
            for (int i = 0; i <= g_vts->buffer_len - g_vts->cursor_pos; i++) vts_write_physical("\x1b[D", 3);
        }
    }
}

static void vts_process_input_char(char ch) {
    if (ch == 0x03) {
        // Ctrl+C
        if (g_vts->config.signal_receiver_task_handle != NULL) {
            Task_SendSignal(g_vts->config.signal_receiver_task_handle, SIG_INTERRUPT);
        }
        memset(g_vts->line_buffer, 0, VTS_LINE_BUFFER_SIZE);
        g_vts->buffer_len = 0;
        g_vts->cursor_pos = 0;
        g_vts->ansi_state = ANSI_STATE_NORMAL;
        vts_write_physical("^C\r\n", 4);
        return;
    }

    if (ch == 0x1A) {
        // Ctrl+Z
        if (g_vts->config.signal_receiver_task_handle != NULL) {
            Task_SendSignal(g_vts->config.signal_receiver_task_handle, SIG_SUSPEND);
        }
        memset(g_vts->line_buffer, 0, VTS_LINE_BUFFER_SIZE);
        g_vts->buffer_len = 0;
        g_vts->cursor_pos = 0;
        g_vts->ansi_state = ANSI_STATE_NORMAL;
        vts_write_physical("^Z\r\n", 4);
        return;
    }

    if (ch == 0x02) {
        // Ctrl+B
        if (g_vts->config.signal_receiver_task_handle != NULL) {
            Task_SendSignal(g_vts->config.signal_receiver_task_handle, SIG_BACKGROUND);
        }
        memset(g_vts->line_buffer, 0, VTS_LINE_BUFFER_SIZE);
        g_vts->buffer_len = 0;
        g_vts->cursor_pos = 0;
        g_vts->ansi_state = ANSI_STATE_NORMAL;
        vts_write_physical("^B\r\n", 4);
        return;
    }

    if (g_vts->terminal_mode == VTS_MODE_RAW) {
        Stream_Write(g_vts->focused_input_stream, &ch, 1, MYRTOS_MAX_DELAY);
        return;
    }
    if (g_vts->ansi_state == ANSI_STATE_NORMAL) {
        if (ch == '\x1b') g_vts->ansi_state = ANSI_STATE_ESC;
        else handle_canonical_input(ch);
    } else if (g_vts->ansi_state == ANSI_STATE_ESC) {
        if (ch == '[') g_vts->ansi_state = ANSI_STATE_BRACKET;
        else g_vts->ansi_state = ANSI_STATE_NORMAL;
    } else if (g_vts->ansi_state == ANSI_STATE_BRACKET) {
        if (ch == 'D') {
            if (g_vts->cursor_pos > 0) {
                g_vts->cursor_pos--;
                vts_write_physical("\x1b[D", 3);
            }
        } else if (ch == 'C') {
            if (g_vts->cursor_pos < g_vts->buffer_len) {
                g_vts->cursor_pos++;
                vts_write_physical("\x1b[C", 3);
            }
        }
        g_vts->ansi_state = ANSI_STATE_NORMAL;
    }
}

static void vts_forward_output(StreamHandle_t stream) {
    char buffer[VTS_RW_BUFFER_SIZE];
    if (!stream) return;
    size_t bytes_read = Stream_Read(stream, buffer, VTS_RW_BUFFER_SIZE, 0);
    if (bytes_read > 0) vts_write_physical(buffer, bytes_read);
}

static void vts_process_background_stream(void) {
    char buffer[VTS_RW_BUFFER_SIZE];
    // 尝试以非阻塞方式读取后台流的数据.
    size_t bytes_read = Stream_Read(g_vts->background_stream, buffer, VTS_RW_BUFFER_SIZE, 0);
    if (bytes_read > 0) {
        vts_write_physical(buffer, bytes_read);
    }
}


static void VTS_Task(void *param) {
    (void) param;
    char input_char;
    for (;;) {
        //处理来自物理终端的输入.
        if (Stream_Read(g_vts->config.physical_stream, &input_char, 1, 0) > 0) {
            Mutex_Lock(g_vts->lock);
            vts_process_input_char(input_char);
            Mutex_Unlock(g_vts->lock);
        }
        //将当前焦点任务的输出转发到物理终端.
        vts_forward_output(g_vts->focused_output_stream);
        //将后台流的所有输出 (主要是系统日志) 转发到物理终端.
        vts_process_background_stream();
        Task_Delay(MS_TO_TICKS(1));
    }
}

int VTS_Init(const VTS_Config_t *config) {
    if (g_vts || !config || !config->physical_stream || !config->root_input_stream || !config->root_output_stream || !
        config->signal_receiver_task_handle) {
        return -1;
    }
    g_vts = (VTS_Instance_t *) MyRTOS_Malloc(sizeof(VTS_Instance_t));
    if (!g_vts) return -1;
    memset(g_vts, 0, sizeof(VTS_Instance_t));
    g_vts->config = *config;
    g_vts->lock = Mutex_Create();
    g_vts->background_stream = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    g_vts->focused_input_stream = config->root_input_stream;
    g_vts->focused_output_stream = config->root_output_stream;
    g_vts->terminal_mode = VTS_MODE_CANONICAL;
    g_vts->vts_task_handle = Task_Create(VTS_Task, "VTSService", VTS_TASK_STACK_SIZE, g_vts, VTS_TASK_PRIORITY);
    if (!g_vts->vts_task_handle) {
        return -1;
    }
    return 0;
}

int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    if (!g_vts) return -1;
    Mutex_Lock(g_vts->lock);
    g_vts->focused_input_stream = input_stream ? input_stream : g_vts->config.root_input_stream;
    g_vts->focused_output_stream = output_stream ? output_stream : g_vts->config.root_output_stream;
    Mutex_Unlock(g_vts->lock);
    return 0;
}

void VTS_ReturnToRootFocus(void) {
    if (!g_vts) return;
    Mutex_Lock(g_vts->lock);
    g_vts->focused_input_stream = g_vts->config.root_input_stream;
    g_vts->focused_output_stream = g_vts->config.root_output_stream;
    g_vts->terminal_mode = VTS_MODE_CANONICAL;
    Mutex_Unlock(g_vts->lock);
}

int VTS_SetTerminalMode(VTS_TerminalMode_t mode) {
    if (!g_vts) return -1;
    g_vts->terminal_mode = mode;
    return 0;
}

VTS_TerminalMode_t VTS_GetTerminalMode(void) {
    return g_vts ? g_vts->terminal_mode : VTS_MODE_CANONICAL;
}



StreamHandle_t VTS_GetBackgroundStream(void) {
    return g_vts ? g_vts->background_stream : NULL;
}

int VTS_SendSignal(uint32_t signal) {
    if (!g_vts || !g_vts->config.signal_receiver_task_handle) {
        return -1;
    }
    Task_SendSignal(g_vts->config.signal_receiver_task_handle, signal);
    return 0;
}

#endif // MYRTOS_SERVICE_VTS_ENABLE
