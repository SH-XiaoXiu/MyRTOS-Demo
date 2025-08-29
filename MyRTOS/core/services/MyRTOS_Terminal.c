//
// Created by XiaoXiu on 8/29/2025.
//

#include "MyRTOS_Terminal.h"
#include "MyRTOS_Console.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Platform.h"
#include <string.h>

#if (MY_RTOS_USE_TERMINAL == 1)


#define TERMINAL_PRINT(...) \
    do { \
        if (MyRTOS_Console_GetMode() == CONSOLE_MODE_TERMINAL) { \
            PRINT(__VA_ARGS__); \
        } \
    } while(0)


typedef struct TerminalCommandNode_t {
    const char *name;
    const char *help;
    TerminalCommandFunc_t function;
    struct TerminalCommandNode_t *next;
} TerminalCommandNode_t;

static TaskHandle_t g_terminal_task_handle = NULL;
static TerminalCommandNode_t *g_command_list_head = NULL;

static void prvTerminalTask(void *pv);

static void prvProcessCommand(char *cmd_buffer);

static void cmd_help(int argc, char **argv); // ���� help ����������

void MyRTOS_Terminal_Init(void) {
    MyRTOS_Terminal_RegisterCommand("help", "Show command list", cmd_help);
}

int MyRTOS_Terminal_Start(void) {
    if (g_terminal_task_handle != NULL) return -1;
    MyRTOS_Console_SetMode(CONSOLE_MODE_TERMINAL);
    g_terminal_task_handle = Task_Create(prvTerminalTask,
                                         "Terminal",
                                         SYS_TERMINAL_TASK_STACK_SIZE,
                                         NULL,
                                         SYS_TERMINAL_TASK_PRIORITY);
    if (g_terminal_task_handle == NULL) {
        SYS_LOGE("Failed to create terminal task.\n");
        MyRTOS_Console_SetMode(CONSOLE_MODE_LOG); // ʧ��ʱ�ָ�ģʽ
        return -1;
    }
    return 0;
}

int MyRTOS_Terminal_Stop(void) {
    if (g_terminal_task_handle == NULL) return -1;

    TaskHandle_t task_to_delete = g_terminal_task_handle;
    g_terminal_task_handle = NULL; // ������������ֹ����

    // ������ɾ������ǰ�л�ģʽ���������������������
    if (MyRTOS_Console_GetMode() == CONSOLE_MODE_TERMINAL) {
        MyRTOS_Console_SetMode(CONSOLE_MODE_LOG);
        TERMINAL_PRINT("\r\n--- Terminal Stopped. Switched to LOG mode. ---\r\n");
    }

    Task_Delete(task_to_delete);
    return 0;
}

int MyRTOS_Terminal_IsRunning(void) {
    return (g_terminal_task_handle != NULL);
}




static void prvTerminalTask(void *pv) {
    char cmd_buffer[SYS_TERMINAL_MAX_CMD_LENGTH];
    int buffer_pos = 0;
    char received_char;

    TERMINAL_PRINT("\r\n"); // ȷ�����п�ʼ
    TERMINAL_PRINT(SYS_TERMINAL_PROMPT);

    while (1) {
        // ֱ�Ӵ�ƽ̨������ʽ�ػ�ȡ�ַ�
        if (MyRTOS_Platform_GetChar_Blocking(&received_char)) {
            // ����ڵȴ��ڼ�ģʽ���л�����������룬�ó�CPU
            if (MyRTOS_Console_GetMode() != CONSOLE_MODE_TERMINAL) {
                Task_Delay(MS_TO_TICKS(100));
                continue;
            }

            // --- �ؼ���BUG�޸����߼��Ż� ---
            if (received_char == '\r' || received_char == '\n') {
                TERMINAL_PRINT("\r\n"); // ͳһ����Ϊ CR+LF�����������ն�

                if (buffer_pos > 0) {
                    cmd_buffer[buffer_pos] = '\0'; // ��ֹ�ַ���
                    prvProcessCommand(cmd_buffer);
                }

                buffer_pos = 0; // ���û�����
                TERMINAL_PRINT(SYS_TERMINAL_PROMPT); // ��ʾ��һ����ʾ��
            }
            // �˸�� (���� Backspace �� Delete)
            else if (received_char == '\b' || received_char == 127) {
                if (buffer_pos > 0) {
                    buffer_pos--;
                    TERMINAL_PRINT("\b \b"); // ���ն���ʵ���Ӿ��˸�Ч��
                }
            }
            // ��ͨ�ɴ�ӡ�ַ�
            else if (buffer_pos < (SYS_TERMINAL_MAX_CMD_LENGTH - 1) && received_char >= ' ' && received_char <= '~') {
                cmd_buffer[buffer_pos++] = received_char;
                TERMINAL_PRINT("%c", received_char); // �����ַ�
            }
        }
    }
}

static void prvProcessCommand(char *cmd_buffer) {
    char *argv[SYS_TERMINAL_MAX_ARGS];
    int argc = 0;

    // ʹ�� strtok �ָ�����Ͳ���
    char *token = strtok(cmd_buffer, " ");
    while (token != NULL && argc < SYS_TERMINAL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) return;

    // ���Ҳ�ִ������
    TerminalCommandNode_t *iterator = g_command_list_head;
    while (iterator != NULL) {
        if (strcmp(argv[0], iterator->name) == 0) {
            iterator->function(argc, argv);
            return;
        }
        iterator = iterator->next;
    }

    TERMINAL_PRINT("Command not found: %s\r\n", argv[0]);
}

int MyRTOS_Terminal_RegisterCommand(const char *name, const char *help, TerminalCommandFunc_t func) {
    if (!name || !help || !func) return -1;

    TerminalCommandNode_t *new_node = MyRTOS_Malloc(sizeof(TerminalCommandNode_t));
    if (!new_node) {
        SYS_LOGE("Failed to allocate memory for cmd: %s", name);
        return -1;
    }
    new_node->name = name;
    new_node->help = help;
    new_node->function = func;
    new_node->next = g_command_list_head;
    g_command_list_head = new_node;

    return 0;
}

static void cmd_help(int argc, char **argv) {
    TERMINAL_PRINT("Available commands:\r\n");
    TerminalCommandNode_t *iterator = g_command_list_head;
    while (iterator != NULL) {
        TERMINAL_PRINT("  %-10s - %s\r\n", iterator->name, iterator->help);
        iterator = iterator->next;
    }
}

#endif // MY_RTOS_USE_TERMINAL
