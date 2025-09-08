#ifndef PLATFORM_PROGRAM_MANAGER_H
#define PLATFORM_PROGRAM_MANAGER_H

#include "platform.h"
#include <stdbool.h>

// 前置声明.
struct ProgramInstance_t;

/**
 * @brief 程序主函数原型.
 * @param argc 参数数量.
 * @param argv 参数值数组.
 */
typedef void (*ProgramMain_t)(int argc, char *argv[]);

/**
 * @brief 定义了一个可执行程序的静态属性.
 */
typedef struct {
    const char *name; // 程序名称, 用于 'run' 命令.
    const char *help; // 程序的简短描述, 用于帮助信息.
    ProgramMain_t main_func; // 程序的入口点函数.
} ProgramDefinition_t;

/**
 * @brief 定义了程序实例的运行模式.
 */
typedef enum {
    PROG_MODE_FOREGROUND, // 前台运行
    PROG_MODE_BACKGROUND // 后台运行 类似守护进程.
} ProgramMode_t;

/**
 * @brief 程序实例的运行状态.
 */
typedef enum {
    PROG_STATE_RUNNING,   // 正在运行
    PROG_STATE_SUSPENDED, // 已挂起
} ProgramState_t;

/**
 * @brief 描述一个正在运行的程序实例.
 */
typedef struct ProgramInstance_t {
    int pid; // 进程ID.
    const ProgramDefinition_t *def; // 静态定义的指针.
    TaskHandle_t task_handle; // 任务句柄.
    ProgramMode_t mode;
    ProgramState_t state;      // 程序的当前运行状态.
    StreamHandle_t stdin_pipe;
    StreamHandle_t stdout_pipe;
    bool is_active; // 标记此实例槽位是否正在使用.
} ProgramInstance_t;




/**
 * @brief 遍历程序实例时使用的回调函数原型.
 * @param instance 指向程序实例的只读指针.
 * @param arg 传递给遍历函数的用户自定义参数.
 * @return 返回 true 继续遍历, false 则停止.
 */
typedef bool (*ProgramInstanceVisitor_t)(const ProgramInstance_t *instance, void *arg);

/**
 * @brief 初始化程序管理器服务.
 * @details 必须在系统启动时调用一次, 先于任何其他程序管理函数. 它负责设置实例池和同步对象.
 */
void Platform_ProgramManager_Init(void);

/**
 * @brief 注册一个可执行程序定义.
 * @param prog 指向一个静态的 ProgramDefinition_t 结构体指针.
 * @return 成功时返回 0, 若注册表已满或prog无效则返回 -1.
 * @note 提供的结构体必须在应用程序的整个生命周期内保持有效.
 */
int Platform_ProgramManager_Register(const ProgramDefinition_t *prog);

/**
 * @brief 安全地遍历所有活动的程序实例.
 * @details 获取锁并为每个活动实例调用访问者回调, 确保了对运行中程序的一致性视图.
 * @param visitor 为每个实例调用的回调函数.
 * @param arg 传递给回调函数的用户自定义参数.
 */
void Platform_ProgramManager_TraverseInstances(ProgramInstanceVisitor_t visitor, void *arg);

/**
 * @brief 启动一个已注册的程序.
 * @details 查找程序定义, 创建任务, 设置IO流并跟踪其生命周期.
 * @param name 要运行的程序名称.
 * @param argc 传递给程序的参数数量.
 * @param argv 传递给程序的参数数组.
 * @param mode 运行模式 (前台或后台).
 * @return 成功时返回指向新实例的临时指针, 失败时返回 NULL.
 * @note 返回的指针不应被长期持有或使用.
 */
ProgramInstance_t *Platform_ProgramManager_Run(const char *name, int argc, char *argv[], ProgramMode_t mode);

/**
 * @brief 为程序提供一种标准的自我终止方式.
 * @details 程序可调用此函数从其代码的任何位置干净地退出, 无需通过多个调用层返回.
 * @param exit_code 退出码 (当前未使用).
 */
void Platform_ProgramManager_Exit(int exit_code);

/**
 * @brief 终止一个正在运行的程序实例.
 * @param pid 要终止的程序的进程ID.
 * @return 若终止请求已成功发送则返回 0, 若未找到PID则返回 -1.
 */
int Platform_ProgramManager_Kill(int pid);


/**
 * @brief 根据PID安全地查找一个程序实例.
 * @param pid 要查找的程序的进程ID.
 * @return 若找到则返回指向该实例的指针, 否则返回 NULL.
 * @note 返回的指针仅在持有 g_prog_manager_lock 期间有效.
 */
ProgramInstance_t *Platform_ProgramManager_GetInstance(int pid);

/**
 * @brief 挂起一个正在运行的程序.
 * @param pid 要挂起的程序的进程ID.
 * @return 成功时返回 0, 若程序未找到或状态不正确则返回 -1.
 */
int Platform_ProgramManager_Suspend(int pid);


/**
 * @brief 恢复一个已挂起的程序.
 * @param pid 要恢复的程序的进程ID.
 * @return 成功时返回 0, 若程序未找到或状态不正确则返回 -1.
 */
int Platform_ProgramManager_Resume(int pid);

#endif // PLATFORM_PROGRAM_MANAGER_H
