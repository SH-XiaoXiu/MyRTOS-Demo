#include "platform_program_manager.h"
#include <string.h>
#include "MyRTOS_Log.h"
#include "MyRTOS_VTS.h"
#include "MyRTOS_Extension.h"


//全局变量
//活动程序实例池.
static ProgramInstance_t g_program_instances[PLATFORM_MAX_RUNNING_PROGRAMS];

//可用程序定义注册表.
static const ProgramDefinition_t *g_registered_programs[MAX_REGISTERED_PROGRAMS];
static size_t g_registered_program_count = 0;

//用于线程安全访问程序实例池的互斥锁.
static MutexHandle_t g_prog_manager_lock = NULL;

//用于分配唯一进程ID的序列器.
static int g_next_pid = 1;

//Shell任务的外部引用, 用于发送信号.
extern TaskHandle_t g_shell_task_h;


//私有函数声明


static void program_launcher_task(void *param);

static const ProgramDefinition_t *find_program_definition(const char *name);

static void program_manager_kernel_event_handler(const KernelEventData_t *pEventData);

static ProgramInstance_t *get_instance_locked(int pid);

//公共 API 实现


//初始化程序管理器服务.
void Platform_ProgramManager_Init(void) {
    g_prog_manager_lock = Mutex_Create();
    if (g_prog_manager_lock == NULL) {
        //致命错误, 管理器在没有锁的情况下无法运行.
        while (1);
    }

    //将所有池和计数器初始化为已知状态.
    memset(g_program_instances, 0, sizeof(g_program_instances));
    memset(g_registered_programs, 0, sizeof(g_registered_programs));
    g_registered_program_count = 0;

    if (MyRTOS_RegisterExtension(program_manager_kernel_event_handler) != 0) {
        //注册失败是致命错误.
        while (1);
    }
}

//注册一个可执行程序定义.
int Platform_ProgramManager_Register(const ProgramDefinition_t *prog) {
    //验证参数.
    if (prog == NULL || prog->name == NULL || prog->main_func == NULL) {
        return -1;
    }

    //检查注册表是否已满.
    if (g_registered_program_count >= MAX_REGISTERED_PROGRAMS) {
        return -1;
    }

    g_registered_programs[g_registered_program_count++] = prog;
    return 0;
}

//安全地遍历所有活动的程序实例.
void Platform_ProgramManager_TraverseInstances(ProgramInstanceVisitor_t visitor, void *arg) {
    if (visitor == NULL) {
        return;
    }

    //加锁以确保对实例池的线程安全遍历.
    Mutex_Lock(g_prog_manager_lock);

    for (int i = 0; i < PLATFORM_MAX_RUNNING_PROGRAMS; ++i) {
        if (g_program_instances[i].is_active) {
            //如果访问者返回 false, 则停止迭代.
            if (!visitor(&g_program_instances[i], arg)) {
                break;
            }
        }
    }

    Mutex_Unlock(g_prog_manager_lock);
}

//启动一个已注册的程序.
ProgramInstance_t *Platform_ProgramManager_Run(const char *name, int argc, char *argv[], ProgramMode_t mode) {
    //查找程序定义.
    const ProgramDefinition_t *def = find_program_definition(name);
    if (def == NULL) {
        LOG_W("ProgManager", "Program '%s' not found.", name);
        return NULL;
    }

    Mutex_Lock(g_prog_manager_lock);

    //在实例池中查找一个空闲槽位.
    int free_slot = -1;
    for (int i = 0; i < PLATFORM_MAX_RUNNING_PROGRAMS; ++i) {
        if (!g_program_instances[i].is_active) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        Mutex_Unlock(g_prog_manager_lock);
        LOG_W("ProgManager", "No free slot to run program '%s'.", name);
        return NULL;
    }

    ProgramInstance_t *new_instance = &g_program_instances[free_slot];
    //清理槽位以防旧数据干扰.
    memset(new_instance, 0, sizeof(ProgramInstance_t));

    //为前台程序创建IO管道.
    if (mode == PROG_MODE_FOREGROUND) {
        new_instance->stdin_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        new_instance->stdout_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);

        if (!new_instance->stdin_pipe || !new_instance->stdout_pipe) {
            //失败时清理资源.
            if (new_instance->stdin_pipe) Pipe_Delete(new_instance->stdin_pipe);
            if (new_instance->stdout_pipe) Pipe_Delete(new_instance->stdout_pipe);
            Mutex_Unlock(g_prog_manager_lock);
            LOG_E("ProgManager", "Failed to create pipes for foreground program.");
            return NULL;
        }
    }
    //创建程序任务.
    TaskHandle_t task_h = Task_Create(program_launcher_task, def->name, PLATFORM_PROGRAM_LAUNCH_STACK, new_instance, 2);

    if (task_h == NULL) {
        if (mode == PROG_MODE_FOREGROUND) {
            Pipe_Delete(new_instance->stdin_pipe);
            Pipe_Delete(new_instance->stdout_pipe);
        }
        Mutex_Unlock(g_prog_manager_lock);
        LOG_E("ProgManager", "Failed to create task for program '%s'.", name);
        return NULL;
    }


    if (mode == PROG_MODE_BACKGROUND) {
        Stream_SetTaskStdIn(task_h, Stream_GetNull());
        Stream_SetTaskStdOut(task_h, Stream_GetNull());
        Stream_SetTaskStdErr(task_h, Stream_GetNull());
    } else {
        // PROG_MODE_FOREGROUND
        Stream_SetTaskStdIn(task_h, new_instance->stdin_pipe);
        Stream_SetTaskStdOut(task_h, new_instance->stdout_pipe);
        Stream_SetTaskStdErr(task_h, new_instance->stdout_pipe);
    }
    //填充实例结构体并标记为活动.
    new_instance->pid = g_next_pid++;
    new_instance->def = def;
    new_instance->task_handle = task_h;
    new_instance->mode = mode;
    new_instance->state = PROG_STATE_RUNNING;
    new_instance->is_active = true;

    Mutex_Unlock(g_prog_manager_lock);

    LOG_D("ProgManager", "Program '%s' started with PID %d.", name, new_instance->pid);
    return new_instance;
}

//为程序提供一种标准的自我终止方式.
void Platform_ProgramManager_Exit(int exit_code) {
    (void) exit_code;
    TaskHandle_t current_task_h = Task_GetCurrentTaskHandle();
    LOG_D("ProgManager", "Program '%s' is explicitly calling Exit().", Task_GetName(current_task_h));
    //对当前任务调用删除可实现干净退出.
    Task_Delete(NULL);
}

//终止一个正在运行的程序实例.
int Platform_ProgramManager_Kill(int pid) {
    TaskHandle_t task_to_kill = NULL;

    //加锁以从共享实例池中安全地查找任务句柄.
    Mutex_Lock(g_prog_manager_lock);

    for (int i = 0; i < PLATFORM_MAX_RUNNING_PROGRAMS; ++i) {
        if (g_program_instances[i].is_active && g_program_instances[i].pid == pid) {
            task_to_kill = g_program_instances[i].task_handle;
            break;
        }
    }

    //立即解锁以防止与内核事件处理器发生死锁.
    Mutex_Unlock(g_prog_manager_lock);

    if (task_to_kill) {
        LOG_D("ProgManager", "Requesting to kill program with PID %d.", pid);
        //请求任务删除, 我们的事件处理器将管理清理工作.
        Task_Delete(task_to_kill);
        return 0;
    }

    LOG_W("ProgManager", "Kill failed: PID %d not found.", pid);
    return -1;
}


// 根据PID查找程序实例.
ProgramInstance_t *Platform_ProgramManager_GetInstance(int pid) {
    ProgramInstance_t *instance = NULL;
    Mutex_Lock(g_prog_manager_lock);
    instance = get_instance_locked(pid);
    Mutex_Unlock(g_prog_manager_lock);
    return instance;
}


int Platform_ProgramManager_Suspend(int pid) {
    int result = -1;
    Mutex_Lock(g_prog_manager_lock);

    ProgramInstance_t *instance = get_instance_locked(pid);
    if (instance && instance->state == PROG_STATE_RUNNING) {
        Task_Suspend(instance->task_handle);
        instance->state = PROG_STATE_SUSPENDED;
        // 一旦挂起, 即使原来是前台任务, 也应被视为后台作业.
        instance->mode = PROG_MODE_BACKGROUND;
        result = 0;
        LOG_D("ProgManager", "Suspended program '%s' (PID: %d).", instance->def->name, instance->pid);
    } else {
        LOG_W("ProgManager", "Suspend failed: PID %d not found or not running.", pid);
    }

    Mutex_Unlock(g_prog_manager_lock);
    return result;
}


int Platform_ProgramManager_Resume(int pid) {
    int result = -1;
    Mutex_Lock(g_prog_manager_lock);

    ProgramInstance_t *instance = get_instance_locked(pid);
    if (instance && instance->state == PROG_STATE_SUSPENDED) {
        Task_Resume(instance->task_handle);
        instance->state = PROG_STATE_RUNNING;
        result = 0;
        LOG_D("ProgManager", "Resumed program '%s' (PID: %d).", instance->def->name, instance->pid);
    } else {
        LOG_W("ProgManager", "Resume failed: PID %d not found or not suspended.", pid);
    }

    Mutex_Unlock(g_prog_manager_lock);
    return result;
}


//私有函数实现


/**
 * @brief 所有程序任务的通用入口点.
 * @details 此包装函数调用实际的程序主函数, 并确保在主函数返回时能正确清理任务资源.
 * @param param 指向该程序的 ProgramInstance_t 的指针.
 */
static void program_launcher_task(void *param) {
    ProgramInstance_t *instance = (ProgramInstance_t *) param;

    //TODO: 按需从实例中解析 argc 和 argv.
    int argc = 0;
    char **argv = NULL;

    //调用实际的程序主函数.
    instance->def->main_func(argc, argv);

    //拦截 main 函数的返回以执行干净的退出.
    LOG_D("ProgLauncher", "Program '%s' (PID: %d) returned from main, performing clean exit.", instance->def->name,
          instance->pid);

    //自我删除, 这会触发内核事件处理器进行清理.
    Task_Delete(NULL);

    //此行代码永远不会执行到.
}

/**
 * @brief 通过名称在注册表中查找程序定义.
 * @param name 要查找的程序名称.
 * @return 若找到则返回定义指针, 否则返回 NULL.
 */
static const ProgramDefinition_t *find_program_definition(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < g_registered_program_count; ++i) {
        if (strcmp(g_registered_programs[i]->name, name) == 0) {
            return g_registered_programs[i];
        }
    }
    return NULL;
}

/**
 * @brief 程序管理器的内核事件处理器.
 * @details 监听任务删除事件, 以回收与已终止程序相关的资源.
 * @param pEventData 指向内核事件数据的指针.
 */
static void program_manager_kernel_event_handler(const KernelEventData_t *pEventData) {
    if (pEventData->eventType == KERNEL_EVENT_TASK_DELETE) {
        TaskHandle_t deleted_task_h = pEventData->task;
        bool was_foreground = false;

        Mutex_Lock(g_prog_manager_lock);

        for (int i = 0; i < PLATFORM_MAX_RUNNING_PROGRAMS; ++i) {
            if (g_program_instances[i].is_active && g_program_instances[i].task_handle == deleted_task_h) {
                ProgramInstance_t *instance = &g_program_instances[i];
                LOG_D("ProgManager", "Event: Task for program '%s' (PID: %d) is being deleted. Cleaning up.",
                      instance->def->name, instance->pid);

                if (instance->mode == PROG_MODE_FOREGROUND) {
                    was_foreground = true;
                    if (instance->stdin_pipe) {
                        Pipe_Delete(instance->stdin_pipe);
                    }
                    if (instance->stdout_pipe) {
                        Pipe_Delete(instance->stdout_pipe);
                    }
                }

                //将实例槽位标记为非活动.
                memset(instance, 0, sizeof(ProgramInstance_t));
                instance->is_active = false;
                break;
            }
        }

        Mutex_Unlock(g_prog_manager_lock);

        //如果一个前台程序已退出, 则通知 shell.
        //必须在解锁后发送信号, 以避免潜在的死锁.
        if (was_foreground && g_shell_task_h) {
            Task_SendSignal(g_shell_task_h, SIG_CHILD_EXIT);
        }
    }
}


// 在持有锁的情况下, 根据PID查找实例.
static ProgramInstance_t *get_instance_locked(int pid) {
    for (int i = 0; i < PLATFORM_MAX_RUNNING_PROGRAMS; ++i) {
        if (g_program_instances[i].is_active && g_program_instances[i].pid == pid) {
            return &g_program_instances[i];
        }
    }
    return NULL;
}
