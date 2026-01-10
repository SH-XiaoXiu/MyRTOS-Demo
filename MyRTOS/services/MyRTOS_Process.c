/**
 * @file  MyRTOS_Process.c
 * @brief MyRTOS 进程管理服务 - 实现
 */
#include "MyRTOS_Process.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1

#include <string.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Extension.h"

#if MYRTOS_SERVICE_LOG_ENABLE == 1
#include "MyRTOS_Log.h"
#else
#define LOG_D(tag, ...) ((void)0)
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

// 全局变量

// 进程实例池
static Process_t g_process_pool[MYRTOS_PROCESS_MAX_INSTANCES];

// 程序注册表
static const ProgramDefinition_t *g_program_registry[MYRTOS_PROCESS_MAX_PROGRAMS];
static size_t g_program_count = 0;

// 进程链表头
static Process_t *g_process_list_head = NULL;

// PID分配器
static pid_t g_next_pid = 1;

// 互斥锁
static MutexHandle_t g_process_lock = NULL;

// Shell任务句柄（用于发送SIGCHLD信号）
#if MYRTOS_SERVICE_VTS_ENABLE == 1 && MYRTOS_SERVICE_SHELL_ENABLE == 1
extern TaskHandle_t g_shell_task_h;
#endif

// 私有函数声明

static void process_launcher_task(void *param);
static void process_cleanup(Process_t *proc);
static void process_kernel_event_handler(const KernelEventData_t *event);
static Process_t *find_process_by_pid_locked(pid_t pid);
static Process_t *find_process_by_task_locked(TaskHandle_t task);
static Process_t *alloc_process_slot_locked(void);

// ============================================
// 进程生命周期管理实现
// ============================================

/**
 * @brief 初始化进程管理服务
 */
void Process_Init(void) {
    // 创建互斥锁
    g_process_lock = Mutex_Create();
    if (g_process_lock == NULL) {
        // 致命错误
        while (1);
    }

    // 初始化进程池
    memset(g_process_pool, 0, sizeof(g_process_pool));
    memset(g_program_registry, 0, sizeof(g_program_registry));
    g_process_list_head = NULL;
    g_program_count = 0;
    g_next_pid = 1;

    // 注册内核事件处理器
    if (MyRTOS_RegisterExtension(process_kernel_event_handler) != 0) {
        // 致命错误
        while (1);
    }

    LOG_I("Process", "Process management service initialized.");
}

/**
 * @brief 创建新进程
 */
pid_t Process_Create(const char *name, ProcessMainFunc main_func,
                     int argc, char *argv[],
                     uint32_t stack_size, uint8_t priority,
                     ProcessMode_t mode) {
    if (name == NULL || main_func == NULL) {
        return -1;
    }

    Mutex_Lock(g_process_lock);

    // 分配进程槽位
    Process_t *proc = alloc_process_slot_locked();
    if (proc == NULL) {
        Mutex_Unlock(g_process_lock);
        LOG_W("Process", "No free slot for process '%s'.", name);
        return -1;
    }

    // 初始化进程结构
    memset(proc, 0, sizeof(Process_t));
    proc->pid = g_next_pid++;
    proc->name = name;
    proc->main_func = main_func;
    proc->argc = argc;
    proc->argv = argv;
    proc->mode = mode;
    proc->state = PROCESS_STATE_RUNNING;
    proc->has_exited = false;
    proc->exit_code = 0;

    // 初始化文件描述符表
    for (int i = 0; i < MYRTOS_PROCESS_MAX_FD; i++) {
        proc->fd_table[i].handle = NULL;
        proc->fd_table[i].type = FD_TYPE_UNUSED;
        proc->fd_table[i].flags = 0;
    }

    // 为前台进程创建管道
#if MYRTOS_SERVICE_VTS_ENABLE == 1
    if (mode == PROCESS_MODE_FOREGROUND) {
        StreamHandle_t stdin_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        StreamHandle_t stdout_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);

        if (stdin_pipe == NULL || stdout_pipe == NULL) {
            if (stdin_pipe) Pipe_Delete(stdin_pipe);
            if (stdout_pipe) Pipe_Delete(stdout_pipe);
            Mutex_Unlock(g_process_lock);
            LOG_E("Process", "Failed to create pipes for process '%s'.", name);
            return -1;
        }

        // 设置标准文件描述符
        proc->fd_table[STDIN_FILENO].handle = stdin_pipe;
        proc->fd_table[STDIN_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDIN_FILENO].flags = O_RDONLY;

        proc->fd_table[STDOUT_FILENO].handle = stdout_pipe;
        proc->fd_table[STDOUT_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDOUT_FILENO].flags = O_WRONLY;

        proc->fd_table[STDERR_FILENO].handle = stdout_pipe;
        proc->fd_table[STDERR_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDERR_FILENO].flags = O_WRONLY;
    } else {
        // 后台进程使用空流
        StreamHandle_t null_stream = Stream_GetNull();
        proc->fd_table[STDIN_FILENO].handle = null_stream;
        proc->fd_table[STDIN_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDIN_FILENO].flags = O_RDONLY;

        proc->fd_table[STDOUT_FILENO].handle = null_stream;
        proc->fd_table[STDOUT_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDOUT_FILENO].flags = O_WRONLY;

        proc->fd_table[STDERR_FILENO].handle = null_stream;
        proc->fd_table[STDERR_FILENO].type = FD_TYPE_STREAM;
        proc->fd_table[STDERR_FILENO].flags = O_WRONLY;
    }
#else
    // 如果没有VTS，使用空流
    StreamHandle_t null_stream = Stream_GetNull();
    proc->fd_table[STDIN_FILENO].handle = null_stream;
    proc->fd_table[STDIN_FILENO].type = FD_TYPE_STREAM;
    proc->fd_table[STDIN_FILENO].flags = O_RDONLY;

    proc->fd_table[STDOUT_FILENO].handle = null_stream;
    proc->fd_table[STDOUT_FILENO].type = FD_TYPE_STREAM;
    proc->fd_table[STDOUT_FILENO].flags = O_WRONLY;

    proc->fd_table[STDERR_FILENO].handle = null_stream;
    proc->fd_table[STDERR_FILENO].type = FD_TYPE_STREAM;
    proc->fd_table[STDERR_FILENO].flags = O_WRONLY;
#endif

    // 创建任务
    TaskHandle_t task = Task_Create(process_launcher_task, name, stack_size, proc, priority);
    if (task == NULL) {
        process_cleanup(proc);
        Mutex_Unlock(g_process_lock);
        LOG_E("Process", "Failed to create task for process '%s'.", name);
        return -1;
    }

    proc->task = task;

    // 设置任务的标准IO流
    Stream_SetTaskStdIn(task, proc->fd_table[STDIN_FILENO].handle);
    Stream_SetTaskStdOut(task, proc->fd_table[STDOUT_FILENO].handle);
    Stream_SetTaskStdErr(task, proc->fd_table[STDERR_FILENO].handle);

    // 添加到进程链表
    proc->next = g_process_list_head;
    g_process_list_head = proc;

    pid_t pid = proc->pid;
    Mutex_Unlock(g_process_lock);

    LOG_D("Process", "Created process '%s' with PID %d.", name, pid);
    return pid;
}

/**
 * @brief 退出当前进程
 */
void exit(int status) {
    Process_Exit(status);
}

void Process_Exit(int status) {
    TaskHandle_t current_task = Task_GetCurrentTaskHandle();

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);

    if (proc != NULL) {
        proc->exit_code = status;
        proc->has_exited = true;
        LOG_D("Process", "Process '%s' (PID %d) exiting with status %d.",
              proc->name, proc->pid, status);
    }
    Mutex_Unlock(g_process_lock);

    // 删除任务（触发清理）
    Task_Delete(NULL);

    // 永不返回
    while (1);
}

/**
 * @brief 终止进程
 */
int Process_Kill(pid_t pid) {
    TaskHandle_t task_to_kill = NULL;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL) {
        task_to_kill = proc->task;
    }
    Mutex_Unlock(g_process_lock);

    if (task_to_kill != NULL) {
        LOG_D("Process", "Killing process PID %d.", pid);
        Task_Delete(task_to_kill);
        return 0;
    }

    LOG_W("Process", "Kill failed: PID %d not found.", pid);
    return -1;
}

/**
 * @brief 挂起进程
 */
int Process_Suspend(pid_t pid) {
    int result = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL && proc->state == PROCESS_STATE_RUNNING) {
        Task_Suspend(proc->task);
        proc->state = PROCESS_STATE_SUSPENDED;
        // 挂起后转为后台
        proc->mode = PROCESS_MODE_BACKGROUND;
        result = 0;
        LOG_D("Process", "Suspended process '%s' (PID %d).", proc->name, proc->pid);
    } else {
        LOG_W("Process", "Suspend failed: PID %d not found or not running.", pid);
    }
    Mutex_Unlock(g_process_lock);

    return result;
}

/**
 * @brief 恢复进程
 */
int Process_Resume(pid_t pid) {
    int result = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL && proc->state == PROCESS_STATE_SUSPENDED) {
        Task_Resume(proc->task);
        proc->state = PROCESS_STATE_RUNNING;
        result = 0;
        LOG_D("Process", "Resumed process '%s' (PID %d).", proc->name, proc->pid);
    } else {
        LOG_W("Process", "Resume failed: PID %d not found or not suspended.", pid);
    }
    Mutex_Unlock(g_process_lock);

    return result;
}

/**
 * @brief 设置进程模式
 */
int Process_SetMode(pid_t pid, ProcessMode_t mode) {
    int result = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL) {
        proc->mode = mode;
        result = 0;
        LOG_D("Process", "Set process '%s' (PID %d) mode to %s.",
              proc->name, proc->pid,
              mode == PROCESS_MODE_FOREGROUND ? "foreground" : "background");
    } else {
        LOG_W("Process", "SetMode failed: PID %d not found.", pid);
    }
    Mutex_Unlock(g_process_lock);

    return result;
}

// ============================================
// 进程信息查询实现
// ============================================

/**
 * @brief 获取当前进程ID
 */
pid_t getpid(void) {
    TaskHandle_t current_task = Task_GetCurrentTaskHandle();
    pid_t pid = 0;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        pid = proc->pid;
    }
    Mutex_Unlock(g_process_lock);

    return pid;
}

/**
 * @brief 获取进程名称
 */
const char *Process_GetName(pid_t pid) {
    const char *name = NULL;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL) {
        name = proc->name;
    }
    Mutex_Unlock(g_process_lock);

    return name;
}

/**
 * @brief 获取进程状态
 */
int Process_GetState(pid_t pid) {
    int state = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL) {
        state = proc->state;
    }
    Mutex_Unlock(g_process_lock);

    return state;
}

/**
 * @brief 获取进程退出码
 */
int Process_GetExitCode(pid_t pid, int *exit_code) {
    int result = -1;

    if (exit_code == NULL) {
        return -1;
    }

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL && proc->has_exited) {
        *exit_code = proc->exit_code;
        result = 0;
    }
    Mutex_Unlock(g_process_lock);

    return result;
}

/**
 * @brief 遍历所有进程
 */
void Process_ForEach(ProcessVisitor_t visitor, void *arg) {
    if (visitor == NULL) {
        return;
    }

    Mutex_Lock(g_process_lock);

    Process_t *proc = g_process_list_head;
    while (proc != NULL) {
        if (!visitor(proc, arg)) {
            break;
        }
        proc = proc->next;
    }

    Mutex_Unlock(g_process_lock);
}

// ============================================
// 文件描述符管理实现
// ============================================

/**
 * @brief 分配文件描述符
 */
int Process_AllocFd(void) {
    TaskHandle_t current_task = Task_GetCurrentTaskHandle();
    int fd = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        // 从3开始（0,1,2是标准IO）
        for (int i = 3; i < MYRTOS_PROCESS_MAX_FD; i++) {
            if (proc->fd_table[i].type == FD_TYPE_UNUSED) {
                fd = i;
                proc->fd_table[i].type = FD_TYPE_STREAM; // 默认类型
                break;
            }
        }
    }
    Mutex_Unlock(g_process_lock);

    return fd;
}

/**
 * @brief 释放文件描述符
 */
void Process_FreeFd(int fd) {
    if (fd < 3 || fd >= MYRTOS_PROCESS_MAX_FD) {
        return; // 不能释放标准IO
    }

    TaskHandle_t current_task = Task_GetCurrentTaskHandle();

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        proc->fd_table[fd].handle = NULL;
        proc->fd_table[fd].type = FD_TYPE_UNUSED;
        proc->fd_table[fd].flags = 0;
    }
    Mutex_Unlock(g_process_lock);
}

/**
 * @brief 设置文件描述符句柄
 */
int Process_SetFdHandle(int fd, void *handle, FdType_t type, uint8_t flags) {
    if (fd < 0 || fd >= MYRTOS_PROCESS_MAX_FD || handle == NULL) {
        return -1;
    }

    TaskHandle_t current_task = Task_GetCurrentTaskHandle();
    int result = -1;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        proc->fd_table[fd].handle = handle;
        proc->fd_table[fd].type = type;
        proc->fd_table[fd].flags = flags;
        result = 0;
    }
    Mutex_Unlock(g_process_lock);

    return result;
}

/**
 * @brief 根据PID获取文件描述符句柄
 */
void *Process_GetFdHandleByPid(pid_t pid, int fd) {
    if (fd < 0 || fd >= MYRTOS_PROCESS_MAX_FD) {
        return NULL;
    }

    void *handle = NULL;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_pid_locked(pid);
    if (proc != NULL) {
        handle = proc->fd_table[fd].handle;
    }
    Mutex_Unlock(g_process_lock);

    return handle;
}

/**
 * @brief 获取当前进程的文件描述符句柄
 */
void *Process_GetFdHandle(int fd) {
    if (fd < 0 || fd >= MYRTOS_PROCESS_MAX_FD) {
        return NULL;
    }

    TaskHandle_t current_task = Task_GetCurrentTaskHandle();
    void *handle = NULL;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        handle = proc->fd_table[fd].handle;
    }
    Mutex_Unlock(g_process_lock);

    return handle;
}

/**
 * @brief 获取文件描述符类型
 */
FdType_t Process_GetFdType(int fd) {
    if (fd < 0 || fd >= MYRTOS_PROCESS_MAX_FD) {
        return FD_TYPE_UNUSED;
    }

    TaskHandle_t current_task = Task_GetCurrentTaskHandle();
    FdType_t type = FD_TYPE_UNUSED;

    Mutex_Lock(g_process_lock);
    Process_t *proc = find_process_by_task_locked(current_task);
    if (proc != NULL) {
        type = proc->fd_table[fd].type;
    }
    Mutex_Unlock(g_process_lock);

    return type;
}

// ============================================
// 程序注册与管理实现
// ============================================

/**
 * @brief 注册程序
 */
int Process_RegisterProgram(const ProgramDefinition_t *prog) {
    if (prog == NULL || prog->name == NULL || prog->main_func == NULL) {
        return -1;
    }

    // 注意：此函数通常在调度器启动前调用，因此不使用互斥锁
    // 如果需要在运行时注册，应添加调度器状态检查

    if (g_program_count >= MYRTOS_PROCESS_MAX_PROGRAMS) {
        LOG_W("Process", "Program registry full.");
        return -1;
    }

    g_program_registry[g_program_count++] = prog;

    LOG_D("Process", "Registered program '%s'.", prog->name);
    return 0;
}

/**
 * @brief 查找程序定义
 */
const ProgramDefinition_t *Process_FindProgram(const char *name) {
    if (name == NULL) {
        return NULL;
    }

    const ProgramDefinition_t *prog = NULL;

    Mutex_Lock(g_process_lock);
    for (size_t i = 0; i < g_program_count; i++) {
        if (strcmp(g_program_registry[i]->name, name) == 0) {
            prog = g_program_registry[i];
            break;
        }
    }
    Mutex_Unlock(g_process_lock);

    return prog;
}

/**
 * @brief 遍历程序定义
 */
void Process_ForEachProgram(ProgramDefinitionVisitor_t visitor, void *arg) {
    if (visitor == NULL) {
        return;
    }

    Mutex_Lock(g_process_lock);
    for (size_t i = 0; i < g_program_count; i++) {
        if (!visitor(g_program_registry[i], arg)) {
            break;
        }
    }
    Mutex_Unlock(g_process_lock);
}

/**
 * @brief 运行程序
 */
pid_t Process_RunProgram(const char *name, int argc, char *argv[], ProcessMode_t mode) {
    const ProgramDefinition_t *prog = Process_FindProgram(name);
    if (prog == NULL) {
        LOG_W("Process", "Program '%s' not found.", name);
        return -1;
    }

    return Process_Create(prog->name, prog->main_func, argc, argv,
                          MYRTOS_PROCESS_LAUNCHER_STACK,
                          MYRTOS_PROCESS_LAUNCHER_PRIORITY,
                          mode);
}

// ============================================
// 私有函数实现
// ============================================

/**
 * @brief 进程启动器任务
 */
static void process_launcher_task(void *param) {
    Process_t *proc = (Process_t *)param;

    // 调用主函数
    int exit_code = proc->main_func(proc->argc, proc->argv);

    // 主函数返回，正常退出
    LOG_D("Process", "Process '%s' (PID %d) returned with code %d.",
          proc->name, proc->pid, exit_code);

    Process_Exit(exit_code);

    // 永不到达
    while (1);
}

/**
 * @brief 清理进程资源
 */
static void process_cleanup(Process_t *proc) {
    if (proc == NULL) {
        return;
    }

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    // 删除管道
    if (proc->mode == PROCESS_MODE_FOREGROUND) {
        if (proc->fd_table[STDIN_FILENO].handle != NULL) {
            Pipe_Delete(proc->fd_table[STDIN_FILENO].handle);
        }
        if (proc->fd_table[STDOUT_FILENO].handle != NULL) {
            Pipe_Delete(proc->fd_table[STDOUT_FILENO].handle);
        }
        // STDERR与STDOUT共享，不需要单独删除
    }
#endif

    // 清零（保持池结构）
    memset(proc, 0, sizeof(Process_t));
}

/**
 * @brief 内核事件处理器
 */
static void process_kernel_event_handler(const KernelEventData_t *event) {
    if (event->eventType != KERNEL_EVENT_TASK_DELETE) {
        return;
    }

    TaskHandle_t deleted_task = event->task;
    bool was_foreground = false;

    Mutex_Lock(g_process_lock);

    // 查找进程
    Process_t *proc = find_process_by_task_locked(deleted_task);
    if (proc != NULL) {
        LOG_D("Process", "Process '%s' (PID %d) is being deleted.", proc->name, proc->pid);

        was_foreground = (proc->mode == PROCESS_MODE_FOREGROUND);

        // 从链表移除
        if (g_process_list_head == proc) {
            g_process_list_head = proc->next;
        } else {
            Process_t *p = g_process_list_head;
            while (p != NULL && p->next != proc) {
                p = p->next;
            }
            if (p != NULL) {
                p->next = proc->next;
            }
        }

        // 清理资源
        process_cleanup(proc);
    }

    Mutex_Unlock(g_process_lock);

    // 通知Shell前台进程退出
#if MYRTOS_SERVICE_VTS_ENABLE == 1 && MYRTOS_SERVICE_SHELL_ENABLE == 1
    if (was_foreground && g_shell_task_h != NULL) {
        Task_SendSignal(g_shell_task_h, SIG_CHILD_EXIT);
    }
#endif
}

/**
 * @brief 根据PID查找进程（需持锁）
 */
static Process_t *find_process_by_pid_locked(pid_t pid) {
    Process_t *proc = g_process_list_head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            return proc;
        }
        proc = proc->next;
    }
    return NULL;
}

/**
 * @brief 根据任务句柄查找进程（需持锁）
 */
static Process_t *find_process_by_task_locked(TaskHandle_t task) {
    Process_t *proc = g_process_list_head;
    while (proc != NULL) {
        if (proc->task == task) {
            return proc;
        }
        proc = proc->next;
    }
    return NULL;
}

/**
 * @brief 分配进程槽位（需持锁）
 */
static Process_t *alloc_process_slot_locked(void) {
    for (int i = 0; i < MYRTOS_PROCESS_MAX_INSTANCES; i++) {
        // 检查task是否为NULL表示槽位空闲
        if (g_process_pool[i].task == NULL) {
            return &g_process_pool[i];
        }
    }
    return NULL;
}

#endif // MYRTOS_SERVICE_PROCESS_ENABLE == 1
