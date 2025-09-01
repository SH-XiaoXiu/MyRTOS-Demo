
#ifndef MYRTOS_SHELL_PRIVATE_H
#define MYRTOS_SHELL_PRIVATE_H

#include "MyRTOS_Service_Config.h"
#include "MyRTOS_Shell.h"


// Shell实例的内部状态
typedef struct ShellInstance_t {
    ShellConfig_t config;
    const ShellCommand_t *commands;
    ShellCommandNode_t *commands_head;
    char cmd_buffer[SHELL_CMD_BUFFER_SIZE];
    int buffer_len;
    int argc;
    char *argv[SHELL_MAX_ARGS];
} ShellInstance_t;


#endif // MYRTOS_SHELL_PRIVATE_H
