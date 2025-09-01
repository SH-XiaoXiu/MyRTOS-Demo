//
// Created by XiaoXiu on 8/30/2025.
//


#include "MyRTOS_Shell.h"


#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include "MyRTOS_IO.h"
#include "MyRTOS_Shell_Private.h"
#include "MyRTOS.h"
#include <string.h>
#include <ctype.h>


/*===========================================================================*
 *                              Private Functions                            *
 *===========================================================================*/

static TaskHandle_t shell_task_h;

// �����������ַ���Ϊ argc/argv
static void parse_command(ShellInstance_t *shell) {
    char *p = shell->cmd_buffer;
    shell->argc = 0;

    while (*p && shell->argc < SHELL_MAX_ARGS) {
        // ����ǰ���ո�
        while (*p && isspace((unsigned char) *p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        // ��¼������ʼλ��
        shell->argv[shell->argc++] = p;
        // Ѱ�Ҳ�������λ�� (�ո���ַ���ĩβ)
        while (*p && !isspace((unsigned char) *p)) {
            p++;
        }
        // ��������ַ���ĩβ������'\0'�ض�
        if (*p) {
            *p++ = '\0';
        }
    }
}

// �ַ���ִ������
static void dispatch_command(ShellInstance_t *shell) {
    if (shell->argc == 0) {
        return;
    }
    ShellCommandNode_t *node = shell->commands_head;
    while (node != NULL) {
        if (strcmp(shell->argv[0], node->name) == 0) {
            node->callback(shell, shell->argc, shell->argv);
            return;
        }
        node = node->next;
    }

    MyRTOS_printf("Command not found: %s\n", shell->argv[0]);
}

// Shell��̨����
// �� MyRTOS_Shell.c ��

static void Shell_Task(void *param) {
    ShellInstance_t *shell = (ShellInstance_t *) param;
    char ch;

    MyRTOS_printf("\nMyRTOS Shell Initialized.\n");
    MyRTOS_printf("%s", shell->config.prompt);

    for (;;) {
        // ���Լ��� stdin ��ȡһ���ַ�
        if (Stream_Read(Task_GetStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY) == 1) {
            if (ch == '\r' || ch == '\n') {
                MyRTOS_printf("\r\n"); // ���Ի���

                shell->cmd_buffer[shell->buffer_len] = '\0';
                if(shell->buffer_len > 0) {
                    parse_command(shell);
                    dispatch_command(shell);
                }

                shell->buffer_len = 0;
                MyRTOS_printf("%s", shell->config.prompt); // ��ӡ����ʾ��
            } else if (ch == '\b' || ch == 127) { // �˸��
                if (shell->buffer_len > 0) {
                    shell->buffer_len--;
                    MyRTOS_printf("\b \b"); // �Լ���������˸�
                }
            } else if (isprint((unsigned char) ch) && shell->buffer_len < SHELL_CMD_BUFFER_SIZE - 1) {
                shell->cmd_buffer[shell->buffer_len++] = ch;
                MyRTOS_putchar(ch); // �Լ���������ַ�
            }
        }
    }
}

/*===========================================================================*
 *                              Public API Implementation                    *
 *===========================================================================*/

ShellHandle_t Shell_Init(const ShellConfig_t *config) {
    if (!config) {
        return NULL;
    }

    ShellInstance_t *shell = (ShellInstance_t *) MyRTOS_Malloc(sizeof(ShellInstance_t));
    if (!shell) {
        return NULL;
    }

    memset(shell, 0, sizeof(ShellInstance_t));
    shell->config = *config;
    shell->commands_head = NULL; // ��ʼ��һ��������

    if (shell->config.prompt == NULL) {
        shell->config.prompt = "> ";
    }

    return shell;
}

TaskHandle_t Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority,
                         uint16_t task_stack_size) {
    if (!shell_h) {
        return NULL;
    }
    shell_task_h = Task_Create(
        Shell_Task,
        task_name,
        task_stack_size,
        shell_h,
        task_priority
    );

    return shell_task_h; // ����������
}

StreamHandle_t Shell_GetStream(ShellHandle_t shell_h) {
    if (!shell_h) {
        return NULL;
    }
    return Task_GetStdOut(shell_task_h);
}

int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback) {
    if (!shell_h || !name || !callback) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;

    // ��������Ƿ��Ѵ���
    ShellCommandNode_t *current = shell->commands_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return -2; // �����Ѵ���
        }
        current = current->next;
    }

    // �����µ�����ڵ�
    ShellCommandNode_t *new_node = (ShellCommandNode_t *) MyRTOS_Malloc(sizeof(ShellCommandNode_t));
    if (!new_node) {
        return -3; // �ڴ����ʧ��
    }
    new_node->name = name;
    new_node->help = help;
    new_node->callback = callback;

    // ���½ڵ���뵽����ͷ�� (��򵥵ķ�ʽ)
    new_node->next = shell->commands_head;
    shell->commands_head = new_node;

    return 0;
}

int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name) {
    if (!shell_h || !name) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;

    ShellCommandNode_t *current = shell->commands_head;
    ShellCommandNode_t *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            if (prev == NULL) {
                // ��ͷ�ڵ�
                shell->commands_head = current->next;
            } else {
                prev->next = current->next;
            }
            MyRTOS_Free(current); // �ͷŽڵ��ڴ�
            return 0;
        }
        prev = current;
        current = current->next;
    }
    return -4; // δ�ҵ�����
}

#endif
