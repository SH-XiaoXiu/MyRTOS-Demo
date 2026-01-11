/**
 * @file  MyRTOS_Process.h
 * @brief MyRTOS 进程管理服务 - 公共接口
 * @details 提供标准化的进程管理功能，包括进程创建、生命周期管理、
 *          文件描述符管理等。参考POSIX标准但针对嵌入式优化。
 */
#ifndef MYRTOS_PROCESS_H
#define MYRTOS_PROCESS_H

#include "MyRTOS_Service_Config.h"

#ifndef MYRTOS_SERVICE_PROCESS_ENABLE
#define MYRTOS_SERVICE_PROCESS_ENABLE 0
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1

#include "MyRTOS.h"
#include "MyRTOS_Stream_Def.h"
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================*
 *                            类型定义                                        *
 *===========================================================================*/

/**
 * @brief 进程ID类型（参考POSIX）
 */
typedef int16_t pid_t;

/**
 * @brief 进程主函数原型（类似标准C的main函数）
 */
typedef int (*ProcessMainFunc)(int argc, char *argv[]);

/**
 * @brief 进程运行模式
 */
typedef enum {
    PROCESS_MODE_FOREGROUND = 0, // 前台运行（占用终端）
    PROCESS_MODE_BACKGROUND = 1  // 后台运行（守护进程风格）
} ProcessMode_t;

/**
 * @brief 进程状态
 */
typedef enum {
    PROCESS_STATE_RUNNING = 0,   // 正在运行
    PROCESS_STATE_SUSPENDED = 1, // 已挂起
    PROCESS_STATE_ZOMBIE = 2     // 僵尸状态（已退出但未回收）
} ProcessState_t;

/**
 * @brief 文件描述符类型（为未来扩展准备）
 */
typedef enum {
    FD_TYPE_UNUSED = 0,   // 未使用
    FD_TYPE_STREAM = 1,   // IO流（当前实现）
    FD_TYPE_FILE = 2,     // 文件（未来支持）
    FD_TYPE_DEVICE = 3    // 设备（未来支持）
} FdType_t;

/**
 * @brief 文件描述符标志（参考POSIX）
 */
#define O_RDONLY 0x01  // 只读
#define O_WRONLY 0x02  // 只写
#define O_RDWR   0x03  // 读写

/**
 * @brief 标准文件描述符编号（POSIX标准）
 */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/**
 * @brief 进程控制块（对外不透明）
 */
typedef struct Process_t Process_t;

/**
 * @brief 进程遍历回调函数
 * @param process 进程指针
 * @param arg 用户参数
 * @return true继续遍历，false停止
 */
typedef bool (*ProcessVisitor_t)(const Process_t *process, void *arg);

/*===========================================================================*
 *                          进程生命周期管理                                   *
 *===========================================================================*/

/**
 * @brief 初始化进程管理服务
 * @details 必须在使用其他API前调用一次
 */
void Process_Init(void);

/**
 * @brief 创建一个新进程
 * @param name 进程名称（用于调试）
 * @param main_func 进程主函数
 * @param argc 参数个数
 * @param argv 参数数组（必须持久有效，进程不会拷贝）
 * @param stack_size 栈大小（字节）
 * @param priority 优先级
 * @param mode 运行模式（前台/后台）
 * @return 成功返回进程ID，失败返回-1
 */
pid_t Process_Create(const char *name, ProcessMainFunc main_func,
                     int argc, char *argv[],
                     uint32_t stack_size, uint8_t priority,
                     ProcessMode_t mode);

/**
 * @brief 退出当前进程（从进程内部调用）
 * @param status 退出状态码
 * @note 符合POSIX标准命名
 */
void exit(int status) __attribute__((noreturn));

/**
 * @brief 退出当前进程（MyRTOS风格别名）
 * @param status 退出状态码
 */
void Process_Exit(int status) __attribute__((noreturn));

/**
 * @brief 终止指定进程（从外部调用）
 * @param pid 要终止的进程ID
 * @return 成功返回0，失败返回-1
 */
int Process_Kill(pid_t pid);

/**
 * @brief 挂起进程
 * @param pid 进程ID
 * @return 成功返回0，失败返回-1
 */
int Process_Suspend(pid_t pid);

/**
 * @brief 恢复进程
 * @param pid 进程ID
 * @return 成功返回0，失败返回-1
 */
int Process_Resume(pid_t pid);

/**
 * @brief 设置进程模式（前台/后台）
 * @param pid 进程ID
 * @param mode 进程模式
 * @return 成功返回0，失败返回-1
 */
int Process_SetMode(pid_t pid, ProcessMode_t mode);

/*===========================================================================*
 *                          进程信息查询                                      *
 *===========================================================================*/

/**
 * @brief 获取当前进程ID（POSIX标准）
 * @return 当前进程的PID，如果当前任务不是进程则返回0
 */
pid_t getpid(void);

/**
 * @brief 获取进程名称
 * @param pid 进程ID
 * @return 进程名称，失败返回NULL
 */
const char *Process_GetName(pid_t pid);

/**
 * @brief 获取进程的Task句柄
 * @param pid 进程ID
 * @return Task句柄，失败返回NULL
 */
TaskHandle_t Process_GetTaskHandle(pid_t pid);

/**
 * @brief 获取进程状态
 * @param pid 进程ID
 * @return 进程状态，失败返回-1
 */
int Process_GetState(pid_t pid);

/**
 * @brief 获取进程退出码
 * @param pid 进程ID
 * @param exit_code 输出退出码
 * @return 成功返回0，失败返回-1
 */
int Process_GetExitCode(pid_t pid, int *exit_code);

/**
 * @brief 遍历所有进程
 * @param visitor 回调函数
 * @param arg 用户参数
 */
void Process_ForEach(ProcessVisitor_t visitor, void *arg);

/*===========================================================================*
 *                      文件描述符管理（为文件系统准备）                         *
 *===========================================================================*/

/**
 * @brief 为当前进程分配一个文件描述符
 * @return 成功返回fd编号(>=3)，失败返回-1
 */
int Process_AllocFd(void);

/**
 * @brief 释放文件描述符
 * @param fd 文件描述符
 */
void Process_FreeFd(int fd);

/**
 * @brief 设置文件描述符的句柄
 * @param fd 文件描述符
 * @param handle 句柄指针（Stream或File等）
 * @param type 类型
 * @param flags 标志
 * @return 成功返回0，失败返回-1
 */
int Process_SetFdHandle(int fd, void *handle, FdType_t type, uint8_t flags);

/**
 * @brief 根据PID获取文件描述符的句柄
 * @param pid 进程ID
 * @param fd 文件描述符
 * @return 句柄指针，失败返回NULL
 */
void *Process_GetFdHandleByPid(pid_t pid, int fd);

/**
 * @brief 获取当前进程的文件描述符句柄
 * @param fd 文件描述符
 * @return 句柄指针，失败返回NULL
 */
void *Process_GetFdHandle(int fd);

/**
 * @brief 获取文件描述符的类型
 * @param fd 文件描述符
 * @return 类型，失败返回FD_TYPE_UNUSED
 */
FdType_t Process_GetFdType(int fd);

/*===========================================================================*
 *                      程序注册与管理（兼容旧接口）                            *
 *===========================================================================*/

/**
 * @brief 程序定义结构体（用于静态注册）
 */
typedef struct {
    const char *name;        // 程序名称
    const char *help;        // 帮助信息
    ProcessMainFunc main_func; // 主函数
} ProgramDefinition_t;

/**
 * @brief 程序定义遍历回调
 */
typedef bool (*ProgramDefinitionVisitor_t)(const ProgramDefinition_t *def, void *arg);

/**
 * @brief 注册一个可执行程序
 * @param prog 程序定义（必须持久有效）
 * @return 成功返回0，失败返回-1
 */
int Process_RegisterProgram(const ProgramDefinition_t *prog);

/**
 * @brief 根据名称查找程序定义
 * @param name 程序名称
 * @return 找到返回定义指针，否则返回NULL
 */
const ProgramDefinition_t *Process_FindProgram(const char *name);

/**
 * @brief 遍历已注册的程序
 * @param visitor 回调函数
 * @param arg 用户参数
 */
void Process_ForEachProgram(ProgramDefinitionVisitor_t visitor, void *arg);

/**
 * @brief 运行已注册的程序（Shell命令使用）
 * @param name 程序名称
 * @param argc 参数个数
 * @param argv 参数数组
 * @param mode 运行模式
 * @return 成功返回PID，失败返回-1
 */
pid_t Process_RunProgram(const char *name, int argc, char *argv[], ProcessMode_t mode);

/*===========================================================================*
 *                      内部结构体定义（仅供查询）                              *
 *===========================================================================*/

/**
 * @brief 进程信息结构体（用于Process_ForEach）
 * @note 这是只读视图，不应修改
 */
struct Process_t {
    pid_t pid;                  // 进程ID
    TaskHandle_t task;          // 底层任务句柄
    const char *name;           // 进程名称
    ProcessMainFunc main_func;  // 进程主函数

    // 参数
    int argc;
    char **argv;

    // 退出信息
    int exit_code;
    bool has_exited;

    // 状态
    ProcessMode_t mode;
    ProcessState_t state;

    // 文件描述符表
    struct {
        void *handle;           // Stream、File或其他
        FdType_t type;
        uint8_t flags;
    } fd_table[MYRTOS_PROCESS_MAX_FD];

    // 链表节点（内部使用）
    struct Process_t *next;
};

#endif // MYRTOS_SERVICE_PROCESS_ENABLE == 1
#endif // MYRTOS_PROCESS_H
