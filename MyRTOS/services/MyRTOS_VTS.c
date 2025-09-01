/**
 * @file  MyRTOS_VTS.c
 * @brief MyRTOS �����ն˷��� (Virtual Terminal Service)
 */
#include "MyRTOS_VTS.h"

#if (MYRTOS_SERVICE_VTS_ENABLE == 1)

#include "MyRTOS_IO.h"
#include <string.h>


//�ڲ����ݽṹ 
typedef struct {
    //�����
    StreamHandle_t physical_stream;
    StreamHandle_t primary_in;
    StreamHandle_t primary_out;
    StreamHandle_t background_in;

    //״̬����
    volatile StreamHandle_t focused_stream;

    MutexHandle_t lock;
} VTS_Instance_t;

//ȫ��ʵ�� 
static VTS_Instance_t *g_vts = NULL;

//VTS ������ 
static void VTS_Task(void *param) {
    VTS_Instance_t *vts = (VTS_Instance_t *)param;
    char buffer[VTS_RW_BUFFER_SIZE];
    size_t bytes_read;
    StreamHandle_t streams_to_drain[] = { vts->primary_out, vts->background_in };

    for (;;) {
        //ת���������뵽������ͨ��
        bytes_read = Stream_Read(vts->physical_stream, buffer, VTS_RW_BUFFER_SIZE, 0);
        if (bytes_read > 0) {
            Stream_Write(vts->primary_in, buffer, bytes_read, MYRTOS_MAX_DELAY);
        }
        //��ȡ��ǰ�������Ŀ���
        Mutex_Lock(vts->lock);
        StreamHandle_t current_focus = vts->focused_stream;
        Mutex_Unlock(vts->lock);
        //�����������ӽ�������ȡ���ݲ�д�������ն�
        if (current_focus) {
            bytes_read = Stream_Read(current_focus, buffer, VTS_RW_BUFFER_SIZE, 0);
            if (bytes_read > 0) {
                Stream_Write(vts->physical_stream, buffer, bytes_read, MYRTOS_MAX_DELAY);
            }
        }
        //�������зǽ��������Է��������߱�����
        for(size_t i = 0; i < sizeof(streams_to_drain)/sizeof(streams_to_drain[0]); ++i) {
            if (streams_to_drain[i] != current_focus) {
                //�������ض�ȡ��������������
                while(Stream_Read(streams_to_drain[i], buffer, VTS_RW_BUFFER_SIZE, 0) > 0);
            }
        }
        Task_Delay(MS_TO_TICKS(10));
    }
}

//����APIʵ�� 
int VTS_Init(StreamHandle_t physical_stream, VTS_Handles_t *handles) {
    if (!physical_stream || !handles) return -1;

    g_vts = (VTS_Instance_t *)MyRTOS_Malloc(sizeof(VTS_Instance_t));
    if (!g_vts) return -1;
    memset(g_vts, 0, sizeof(VTS_Instance_t));

    g_vts->physical_stream = physical_stream;
    g_vts->primary_in = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    g_vts->primary_out = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    g_vts->background_in = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    g_vts->lock = Mutex_Create();

    if (!g_vts->primary_in || !g_vts->primary_out || !g_vts->background_in || !g_vts->lock) {
        if (g_vts->primary_in) Pipe_Delete(g_vts->primary_in);
        if (g_vts->primary_out) Pipe_Delete(g_vts->primary_out);
        if (g_vts->background_in) Pipe_Delete(g_vts->background_in);
        if (g_vts->lock) Mutex_Delete(g_vts->lock);
        MyRTOS_Free(g_vts);
        g_vts = NULL;
        return -1;
    }
    //Ĭ�����ã���Ҫ����ͨ��ӵ�н���
    g_vts->focused_stream = g_vts->primary_out;
    //���ؾ��
    handles->primary_input_stream = g_vts->primary_in;
    handles->primary_output_stream = g_vts->primary_out;
    handles->background_stream = g_vts->background_in;
    //��������
    TaskHandle_t task_h = Task_Create(VTS_Task, "VTSService", VTS_TASK_STACK_SIZE, g_vts, VTS_TASK_PRIORITY);
    if (!task_h) {
        Pipe_Delete(g_vts->primary_in);
        Pipe_Delete(g_vts->primary_out);
        Pipe_Delete(g_vts->background_in);
        Mutex_Delete(g_vts->lock);
        MyRTOS_Free(g_vts);
        g_vts = NULL;
        return -1;
    }
    return 0;
}

int VTS_SetFocus(StreamHandle_t stream) {
    if (!g_vts) return -1;
    //������������ΪNULL����Ĭģʽ��
    if (stream != NULL) {
        //��� stream �Ƿ���VTS����������֮һ
        if (stream != g_vts->primary_out && stream != g_vts->background_in) {
            return -1;
        }
    }
    Mutex_Lock(g_vts->lock);
    g_vts->focused_stream = stream;
    Mutex_Unlock(g_vts->lock);
    return 0;
}

#endif