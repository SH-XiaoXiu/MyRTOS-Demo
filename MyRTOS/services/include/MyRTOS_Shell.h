/**
 * @file  MyRTOS_Shell.h
 * @brief MyRTOS ������(Shell)���� - �����ӿ�
 * @details �ṩһ���ɽ����������н��棬����ִ���û�������������IO��ģ�顣
 */
#ifndef MYRTOS_SHELL_H
#define MYRTOS_SHELL_H

#include "MyRTOS.h"
#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_SHELL_ENABLE
#define MYRTOS_SERVICE_SHELL_ENABLE 0
#endif

#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include "MyRTOS_Stream_Def.h"

// ǰ������Shellʵ���ṹ��
struct ShellInstance_t;
/** @brief Shellʵ��������� */
typedef struct ShellInstance_t *ShellHandle_t;

/**
 * @brief Shell����ص�����ԭ�͡�
 * @param shell_h   [in] ִ�д������Shellʵ�������
 * @param argc      [in] �������� (���������)��
 * @param argv      [in] �����ַ������顣
 * @return int      ����ִ�еķ���ֵ (ͨ�� 0 ��ʾ�ɹ�)��
 */
typedef int (*ShellCommandCallback_t)(ShellHandle_t shell_h, int argc, char *argv[]);

/**
 * @brief Shell�����ṹ�塣
 */
typedef struct {
    const char *name; // ��������
    const char *help; // ����İ�����Ϣ
    ShellCommandCallback_t callback; // �������
} ShellCommand_t;

/**
 * @brief Shell����ڶ���ṹ�塣
 */
typedef struct ShellCommandNode_t {
    const char *name;
    const char *help;
    ShellCommandCallback_t callback;
    struct ShellCommandNode_t *next; // ָ����һ������ڵ��ָ��
} ShellCommandNode_t;

/**
 * @brief Shell ��ʼ�����ýṹ�� (��������ע��)��
 */
typedef struct {
    const char *prompt; // ��������ʾ�� (���� "> ")
} ShellConfig_t;


/**
 * @brief ��������ʼ��һ��Shell����ʵ����
 * @param config        [in] ָ��Shell���õ�ָ�� (��������ע�����)��
 * @param commands      [in] ָ������������ָ�롣
 * @param command_count [in] ������е�����������
 * @return ShellHandle_t    �ɹ��򷵻�Shellʵ�����, ʧ�ܷ���NULL��
 */
ShellHandle_t Shell_Init(const ShellConfig_t *config);

/**
 * @brief ����Shell���������
 * @details �˺����ᴴ��һ����̨����������Shell��������պʹ���ѭ����
 * @param shell_h       [in] Shell_Init ���صľ����
 * @param task_name     [in] Shell��������ơ�
 * @param task_priority [in] Shell��������ȼ���
 * @param task_stack_size [in] Shell�����ջ��С��
 * @return TaskHandle_t   �ɹ��򷵻ش�����������, ʧ�ܷ���NULL��
 */
TaskHandle_t Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority, uint16_t task_stack_size);

/**
 * @brief ��ȡ��Shellʵ���������������
 * @details ���������ص�������Ҫֱ��д���ն˷ǳ����á�
 * @param shell_h [in] Shellʵ�������
 * @return StreamHandle_t �������
 */
StreamHandle_t Shell_GetStream(ShellHandle_t shell_h);


int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback);

int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name);

#endif // MYRTOS_SHELL_ENABLE

#endif // MYRTOS_SHELL_H
