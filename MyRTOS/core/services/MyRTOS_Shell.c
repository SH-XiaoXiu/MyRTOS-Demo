//
// Created by XiaoXiu on 8/29/2025.
//

#include "MyRTOS.h"
#include "MyRTOS_Shell.h"

#if (MY_RTOS_USE_SHELL == 1)

#include "MyRTOS_IO.h"
#include <string.h>

#include "MyRTOS_Log.h"
#include "MyRTOS_Std.h"

typedef struct ShellCommandNode_t {
    const char *name;
    const char *help;
    ShellCommandFunc_t function;
    struct ShellCommandNode_t *next;
} ShellCommandNode_t;

static TaskHandle_t g_shell_task_handle = NULL;
static ShellCommandNode_t *g_command_list_head = NULL;

static void prvShellTask(void *pv);

static void prvProcessCommand(char *cmd_buffer);

static void cmd_help(int argc, char **argv);

static void cmd_exit(int argc, char **argv);

/**
 * @brief ��ʼ�� Shell ����, ע����������
 */
void MyRTOS_Shell_Init(void) {
    MyRTOS_Shell_RegisterCommand("help", "Show command list", cmd_help);
    MyRTOS_Shell_RegisterCommand("exit", "Exit the shell session and stop the service", cmd_exit);
}

/**
 * @brief ���� Shell ����
 */
int MyRTOS_Shell_Start(void) {
    if (g_shell_task_handle != NULL) {
        return 0; // ������������
    }

    g_shell_task_handle = Task_Create(prvShellTask,
                                      "Shell",
                                      SYS_SHELL_TASK_STACK_SIZE,
                                      NULL,
                                      SYS_SHELL_TASK_PRIORITY);
    if (g_shell_task_handle == NULL) {
        MyRTOS_printf("Error: Failed to start Shell service.\n");
        return -1;
    }

    SYS_LOGD("Shell service started.");
    return 0;
}

/**
 * @brief ֹͣ Shell ����
 */
int MyRTOS_Shell_Stop(void) {
    if (g_shell_task_handle == NULL) {
        return 0; // ������ֹͣ
    }
    // ��Ҫһ����ʱ�������ΪTask_Delete��ʹg_shell_task_handleʧЧ
    TaskHandle_t task_to_delete = g_shell_task_handle;
    g_shell_task_handle = NULL; // ����������

    Task_Delete(task_to_delete);
    SYS_LOGD("Shell service stopped.");
    return 0;
}

/**
 * @brief ��ѯ Shell �����Ƿ���������
 */
int MyRTOS_Shell_IsRunning(void) {
    // ������Ƿ���Ч�����������Ƿ���������
    if (g_shell_task_handle != NULL && Task_GetState(g_shell_task_handle) != TASK_STATE_UNUSED) {
        return 1;
    }
    // ��������Ч������״̬��UNUSED��˵�������쳣�˳��ˣ�ҲӦ��������
    if (g_shell_task_handle != NULL) {
        g_shell_task_handle = NULL;
    }
    return 0;
}


/**
 * @brief Shell ��������
 */
static void prvShellTask(void *pv) {
    char cmd_buffer[SYS_SHELL_MAX_CMD_LENGTH];

    MyRTOS_printf("\nMyRTOS Shell started. Type 'help' for commands.\n");

    while (1) {
        MyRTOS_printf(SYS_SHELL_PROMPT);
        if (MyRTOS_gets(cmd_buffer, sizeof(cmd_buffer)) != NULL) {
            if (strlen(cmd_buffer) > 0) {
                prvProcessCommand(cmd_buffer);
            }
        } else {
            MyRTOS_printf("\nError: stdin stream closed or error. Shell task exiting.\n");
            break;
        }
    }

    // �����������쳣�˳�ѭ�����������
    g_shell_task_handle = NULL;
    Task_Delete(NULL);
}

static void prvProcessCommand(char *cmd_buffer) {
    char *argv[SYS_SHELL_MAX_ARGS];
    int argc = 0;
    cmd_buffer[strcspn(cmd_buffer, "\r\n")] = 0;
    char *token = strtok(cmd_buffer, " ");
    while (token != NULL && argc < SYS_SHELL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    if (argc == 0) return;
    ShellCommandNode_t *iterator = g_command_list_head;
    while (iterator != NULL) {
        if (strcmp(argv[0], iterator->name) == 0) {
            iterator->function(argc, argv);
            return;
        }
        iterator = iterator->next;
    }
    MyRTOS_printf("Command not found: %s\n", argv[0]);
}

int MyRTOS_Shell_RegisterCommand(const char *name, const char *help, ShellCommandFunc_t func) {
    if (!name || !help || !func) return -1;
    ShellCommandNode_t *new_node = MyRTOS_Malloc(sizeof(ShellCommandNode_t));
    if (!new_node) return -1;
    new_node->name = name;
    new_node->help = help;
    new_node->function = func;
    new_node->next = g_command_list_head;
    g_command_list_head = new_node;
    return 0;
}

static void cmd_help(int argc, char **argv) {
    MyRTOS_printf("Available commands:\n");
    ShellCommandNode_t *iterator = g_command_list_head;
    while (iterator != NULL) {
        MyRTOS_printf("  %-10s - %s\n", iterator->name, iterator->help);
        iterator = iterator->next;
    }
}


/**
 * @brief ���õ� exit ����ʵ��
 *        ��ͨ�����÷���ֹͣAPI�������Լ�
 */
static void cmd_exit(int argc, char **argv) {
    (void) argc;
    (void) argv;
    printf("Shell service stopping...\n");
    Task_Delay(MS_TO_TICKS(10)); // ȷ����Ϣ����
    MyRTOS_Shell_Stop();
}

#endif // MY_RTOS_USE_SHELL
