/**
 * @file  MyRTOS_VTS.c
 * @brief MyRTOS 虚拟终端服务 (Virtual Terminal Service)
 */
#include "MyRTOS_VTS.h"

#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
#include <string.h>

// 内部数据结构
typedef struct {
    VTS_Config_t config; // 保存初始化配置

    // 内部状态
    volatile bool log_all_mode;
    char back_cmd_buffer[VTS_MAX_BACK_CMD_LEN];
    size_t back_cmd_buffer_idx;

    // 当前焦点流
    volatile StreamHandle_t focused_stream;

    // RTOS 对象
    TaskHandle_t vts_task_handle;
    MutexHandle_t lock;
    StreamHandle_t background_stream; // 后台任务的统一输出流
} VTS_Instance_t;


// 全局实例指针
static VTS_Instance_t *g_vts = NULL;


// 私有辅助函数

/**
 * @brief 处理从物理终端接收到的单个字符。
 * @details 检查字符是否匹配'back'命令序列。如果不匹配，则将其转发到根输入流。
 * @param vts VTS实例指针。
 * @param ch 接收到的字符。
 */
static void vts_process_input_char(VTS_Instance_t *vts, char ch) {
    if (ch == vts->config.back_command_sequence[vts->back_cmd_buffer_idx]) {
        vts->back_cmd_buffer[vts->back_cmd_buffer_idx++] = ch;

        if (vts->back_cmd_buffer_idx == vts->config.back_command_len) {
            // 完整匹配'back'命令
            vts->back_cmd_buffer_idx = 0; // 重置缓冲区

            // 将焦点重置回根流
            Mutex_Lock(vts->lock);
            vts->focused_stream = vts->config.root_output_stream;
            Mutex_Unlock(vts->lock);

            // 调用回调通知平台层
            if (vts->config.on_back_command) {
                vts->config.on_back_command();
            }
        }
    } else {
        if (vts->back_cmd_buffer_idx > 0) {
            Stream_Write(vts->config.root_input_stream, vts->back_cmd_buffer, vts->back_cmd_buffer_idx,
                         MYRTOS_MAX_DELAY);
        }
        Stream_Write(vts->config.root_input_stream, &ch, 1, MYRTOS_MAX_DELAY);
        vts->back_cmd_buffer_idx = 0;
    }
}

static void vts_forward_output(VTS_Instance_t *vts, StreamHandle_t stream, char *buffer, size_t buffer_size) {
    size_t bytes_read = Stream_Read(stream, buffer, buffer_size, 0);
    if (bytes_read > 0) {
        Stream_Write(vts->config.physical_stream, buffer, bytes_read, MYRTOS_MAX_DELAY);
    }
}

static void vts_drain_stream(StreamHandle_t stream, char *buffer, size_t buffer_size) {
    while (Stream_Read(stream, buffer, buffer_size, 0) > 0);
}


// VTS 主任务

static void VTS_Task(void *param) {
    VTS_Instance_t *vts = (VTS_Instance_t *) param;
    char buffer[VTS_RW_BUFFER_SIZE];
    char input_char;

    for (;;) {
        // 处理物理输入
        if (Stream_Read(vts->config.physical_stream, &input_char, 1, 0) > 0) {
            vts_process_input_char(vts, input_char);
        }

        // 处理输出
        Mutex_Lock(vts->lock);
        bool log_all = vts->log_all_mode;
        StreamHandle_t current_focus_stream = vts->focused_stream;
        Mutex_Unlock(vts->lock);

        if (log_all) {
            vts_forward_output(vts, vts->config.root_output_stream, buffer, VTS_RW_BUFFER_SIZE);
            vts_forward_output(vts, vts->background_stream, buffer, VTS_RW_BUFFER_SIZE);
            if (current_focus_stream != vts->config.root_output_stream) {
                vts_forward_output(vts, current_focus_stream, buffer, VTS_RW_BUFFER_SIZE);
            }
        } else {
            vts_forward_output(vts, current_focus_stream, buffer, VTS_RW_BUFFER_SIZE);
            if (vts->config.root_output_stream != current_focus_stream) {
                vts_drain_stream(vts->config.root_output_stream, buffer, VTS_RW_BUFFER_SIZE);
            }
            if (vts->background_stream != current_focus_stream) {
                vts_drain_stream(vts->background_stream, buffer, VTS_RW_BUFFER_SIZE);
            }
        }

        Task_Delay(MS_TO_TICKS(10));
    }
}


// 公共 API 实现

int VTS_Init(const VTS_Config_t *config) {
    if (g_vts || !config || !config->physical_stream || !config->root_input_stream || !config->root_output_stream ||
        !config->back_command_sequence || config->back_command_len == 0 ||
        config->back_command_len > VTS_MAX_BACK_CMD_LEN) {
        return -1;
    }

    g_vts = (VTS_Instance_t *) MyRTOS_Malloc(sizeof(VTS_Instance_t));
    if (!g_vts)
        return -1;
    memset(g_vts, 0, sizeof(VTS_Instance_t));

    g_vts->config = *config;
    g_vts->lock = Mutex_Create();
    g_vts->background_stream = Pipe_Create(VTS_PIPE_BUFFER_SIZE);

    if (!g_vts->lock || !g_vts->background_stream) {
        goto cleanup;
    }

    // 默认焦点是根流
    g_vts->focused_stream = config->root_output_stream;

    g_vts->vts_task_handle = Task_Create(VTS_Task, "VTSService", VTS_TASK_STACK_SIZE, g_vts, VTS_TASK_PRIORITY);
    if (!g_vts->vts_task_handle) {
        goto cleanup;
    }

    return 0;

cleanup:
    if (g_vts->lock)
        Mutex_Delete(g_vts->lock);
    if (g_vts->background_stream)
        Pipe_Delete(g_vts->background_stream);
    MyRTOS_Free(g_vts);
    g_vts = NULL;
    return -1;
}

int VTS_SetFocus(StreamHandle_t output_stream) {
    if (!g_vts)
        return -1;

    Mutex_Lock(g_vts->lock);
    g_vts->focused_stream = output_stream ? output_stream : g_vts->config.root_output_stream;
    Mutex_Unlock(g_vts->lock);
    return 0;
}

void VTS_ReturnToRootFocus(void) {
    if (!g_vts)
        return;

    Mutex_Lock(g_vts->lock);
    g_vts->focused_stream = g_vts->config.root_output_stream;
    Mutex_Unlock(g_vts->lock);
}

void VTS_SetLogAllMode(bool enable) {
    if (g_vts) {
        g_vts->log_all_mode = enable;
    }
}

StreamHandle_t VTS_GetBackgroundStream(void) {
    if (g_vts) {
        return g_vts->background_stream;
    }
    return NULL;
}

#endif
