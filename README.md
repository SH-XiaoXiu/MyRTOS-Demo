# MyRTOS

一个基于 Cortex-M4 的多任务调度系统。经过全新架构重构，实现了内核、服务与平台驱动的深度解耦。其结构清晰、API 统一、可移植性强，并提供了的内核扩展钩子，适合作为学习实时操作系统原理、研究可移植系统设计的参考项目。

## ✨ 功能特性

*   **分层架构**：
  *   **Kernel (内核层)**：平台无关的核心调度与IPC机制。
  *   **Services (服务层)**：基于内核API实现的可选功能模块，如日志、监视器等。
  *   **Platform (平台层)**：封装所有与硬件相关的代码，实现轻松移植。
*   **多任务调度**：
  *   基于优先级的抢占式调度。
  *   同优先级时间片轮转调度。
*   **IPC机制 (Inter-Process Communication)**：
  *   **消息队列 (Queue)**：线程安全，支持阻塞、非阻塞及超时。
  *   **计数信号量 (Semaphore)**：用于资源计数与任务同步，支持ISR版本。
  *   **互斥锁 (Mutex)**：包含优先级继承协议以防止优先级反转。
  *   **递归互斥锁 (Recursive Mutex)**：允许同一任务嵌套持有。
  *   **任务通知 (Task Notification)**：轻量级的直接到任务的事件传递机制，支持ISR版本。
*   **中断管理**：
  *   提供`FromISR`版本的API，用于在中断服务程序中安全操作内核对象。
  *   支持临界区嵌套。
  *   采用延迟调度机制（PendSV），优化中断响应。
*   **时间管理**：
  *   基于系统Tick的任务延时。
  *   软件定时器服务，支持周期性与一次性定时器。
*   **内存管理**：
  *   基于静态内存池的动态内存分配 (`MyRTOS_Malloc`/`MyRTOS_Free`)。
  *   线程安全，支持空闲块自动合并以减少碎片。
*   **任务管理**：
  *   支持任务的动态创建与删除。
  *   实现任务ID的回收与复用。
  *   提供API以获取任务状态与优先级。
*   **系统监控与调试**：
  *   实时性能监视器 (Monitor)：显示任务状态、优先级、栈使用高水位线、CPU占用率等。
  *   堆内存监控：显示总大小、当前剩余及历史最小剩余。
  *   HardFault异常信息打印。
*   **内核扩展机制 (Hooks)**：
  *   通过`MyRTOS_RegisterExtension`注册回调，可监听内核关键事件（如任务切换、内存分配/释放、系统Tick等）。
  *   为系统调试、性能分析和功能扩展提供了支持。
*   **统一的配置中心**：
  *   通过单一头文件 (`MyRTOS_Config.h`) 对内核功能、服务模块和平台资源进行集中配置。

## 🏗️ 抽象架构

![架构图](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/framework.svg)

## 📁 目录结构

新的目录结构清晰地体现了分层解耦的设计思想：

```
MyRTOS-Project/
├── Firmware/                   # 芯片官方库与核心文件
├── MyRTOS/                   # MyRTOS 操作系统源码
│   ├── kernel/                 # 内核实现 (平台无关)
│   │   ├── include/            # 内核私有头文件
│   │   └── MyRTOS.c
│   ├── platform/               # 平台包 (可替换)
│   │   └── GD32F4xx/           # 针对 GD32F4xx 的具体实现
│   │       ├── include/
│   │       └── *.c
│   ├── services/               # 服务实现 (平台无关)
│   │   ├── include/            # 服务模块头文件
│   │   └── *.c
│   └── MyRTOS_Config_Example.h # 供用户参考的配置文件模板
├── Project/                    # Keil MDK 或其他 IDE 的工程文件
└── User/                       # 用户应用代码
    ├── main.c                  # 应用入口
    └── MyRTOS_Config.h         # RTOS 功能及平台配置文件 (用户唯一配置入口)
```

*   **MyRTOS**: RTOS的全部源码。
  *   **kernel**: **内核实现**。包含任务调度、内存管理、IPC机制等纯软件逻辑，完全与硬件平台无关。它向服务层和用户应用暴露所有核心API。
  *   **services**: **服务实现**。包含日志(Log)、系统监视器(Monitor)、软件定时器(Timer)等可选的功能模块。它们依赖内核API，但与具体硬件平台无关。
  *   **platform**: **平台包**，是连接上层软件与底层硬件的桥梁，是RTOS可移植性的关键。当前提供了一个针对 **GD32F4xx** 的实现包，其内部包含了CPU核心移植代码（上下文切换、中断处理）、板级支持代码（如调试串口`PutChar`实现）等。
*   **User**: 用户的最终应用代码。
  *   **main.c**: 应用的入口点。现在它变得非常简洁，只需初始化系统，创建初始任务，然后启动调度器。
  *   **MyRTOS\_Config.h**: **用户唯一的配置中心**。用户通过复制和修改 `MyRTOS/MyRTOS_Config_Example.h` 模板来创建此文件。它控制着RTOS的所有功能开关、资源限制和平台定义，是整个项目的“指挥棒”。

## 📚 API 列表

所有公开 API 均定义在 `MyRTOS/kernel/include/MyRTOS.h` 和 `MyRTOS/services/include/*.h` 中。

#### 系统核心 (System Core)

*   `void MyRTOS_Init(void)` — 初始化系统核心。
*   `void Task_StartScheduler(void (*idle_task_hook)(void *))` — 启动任务调度器。
*   `uint64_t MyRTOS_GetTick(void)` — 获取当前系统Tick。
*   `uint8_t MyRTOS_Schedule_IsRunning(void)` — 检查调度器是否已启动。
*   `void *MyRTOS_Malloc(size_t wantedSize)` — 内核内存分配。
*   `void MyRTOS_Free(void *pv)` — 内核内存释放。

#### 任务管理 (Task Management)

*   `TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param, uint8_t priority)` — 创建任务。
*   `int Task_Delete(TaskHandle_t task_h)` — 删除任务。
*   `void Task_Delay(uint32_t tick)` — 任务延时。
*   `int Task_Notify(TaskHandle_t task_h)` — 向任务发送通知。
*   `int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken)` — 在ISR中向任务发送通知。
*   `void Task_Wait(void)` — 等待任务通知。
*   `TaskState_t Task_GetState(TaskHandle_t task_h)` — 获取任务状态。
*   `uint8_t Task_GetPriority(TaskHandle_t task_h)` — 获取任务优先级。
*   `TaskHandle_t Task_GetCurrentTaskHandle(void)` — 获取当前任务句柄。

#### 消息队列 (Queue Management)

*   `QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize)` — 创建队列。
*   `void Queue_Delete(QueueHandle_t delQueue)` — 删除队列。
*   `int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks)` — 发送数据到队列 (阻塞/非阻塞/超时)。
*   `int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks)` — 从队列接收数据 (阻塞/非阻塞/超时)。

#### 互斥锁 (Mutex Management)

*   `MutexHandle_t Mutex_Create(void)` — 创建互斥锁。
*   `void Mutex_Delete(MutexHandle_t mutex)` — 删除互斥锁。
*   `void Mutex_Lock(MutexHandle_t mutex)` — 获取互斥锁 (永久等待)。
*   `int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks)` — 在指定时间内尝试获取互斥锁。
*   `void Mutex_Unlock(MutexHandle_t mutex)` — 释放互斥锁。
*   `void Mutex_Lock_Recursive(MutexHandle_t mutex)` — 递归获取互斥锁。
*   `void Mutex_Unlock_Recursive(MutexHandle_t mutex)` — 递归释放互斥锁。

#### 信号量 (Semaphore Management)

*   `SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount)` — 创建计数信号量。
*   `void Semaphore_Delete(SemaphoreHandle_t semaphore)` — 删除信号量。
*   `int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks)` — 获取信号量 (P操作)。
*   `int Semaphore_Give(SemaphoreHandle_t semaphore)` — 释放信号量 (V操作)。
*   `int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken)` — 在ISR中释放信号量。

#### 内核扩展 (Kernel Extension)

*   `int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback)` — 注册一个内核事件回调钩子。
*   `int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback)` — 注销一个内核事件回调钩子。

#### 核心宏

*   `MS_TO_TICKS(ms)` — 毫秒转 ticks。
*   `TICK_TO_MS(tick)` — ticks 转毫秒。
*   `MyRTOS_Port_EnterCritical()` — 进入临界区。
*   `MyRTOS_Port_ExitCritical()` — 退出临界区。
*   `MyRTOS_Port_Yield()` — 手动触发任务调度。

## 🚀 快速开始 & 示例解析

新的架构通过平台钩子（Platform Hooks）极大地简化了用户应用的组织方式。现在，您只需要在平台层提供的特定函数中填充您的业务逻辑即可。

#### 1\. 极简的 \`main\` 函数

在新的架构下，用户的 \`main.c\` 变得异常简洁。所有的硬件初始化、服务配置和RTOS启动都由平台层统一管理(非强制)。

```c
int main(void) {
    // 初始化平台层 (它会处理所有底层细节和RTOS服务)
    Platform_Init();

    // 打印启动信息
    LOG_I("Main", "=========   MyRTOS 演示   =========");
    LOG_I("Main", "系统启动中...");

    // 启动RTOS调度器 (由平台层接管)
    Platform_StartScheduler();

    return 0; // 永远不会执行到这里
}
```

#### 2\. 平台钩子 (Platform Hooks) - 应用的真正入口

您的应用代码现在被组织在几个平台钩子函数中。\`main.c\` 的主要工作就是实现这些钩子。

*   `void Platform_BSP_Init_Hook(void)`: 用于初始化特定于您开发板的硬件，如LED、按键等。
*   `void Platform_CreateTasks_Hook(void)`: 这是**最重要的钩子**，用于创建所有的RTOS对象，包括任务、队列、信号量、定时器等。
*   `void Platform_AppSetup_Hook(ShellHandle_t shell_h)`: 用于在RTOS服务（如Shell）初始化后，进行应用层面的配置，例如注册自定义Shell命令。

下面是 `Platform_CreateTasks_Hook` 的一个示例，展示了如何在这里集中创建所有应用组件：

```c
void Platform_CreateTasks_Hook(void) {
    /* --- 软件定时器测试 --- */
    single_timer_h = Timer_Create("单次定时器", MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    perio_timer_h = Timer_Create("周期定时器", MS_TO_TICKS(10000), 1, perio_timer_cb, NULL);
    Timer_Start(single_timer_h, 0);
    Timer_Start(perio_timer_h, 0);

    /* --- 队列测试 (生产者-消费者) --- */
    product_queue = Queue_Create(3, sizeof(Product_t));
    Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
    Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);

    /* --- 信号量测试 (共享资源) --- */
    printer_semaphore = Semaphore_Create(2, 2); // 假设有2台打印机
    Task_Create(printer_task, "PrinterTask1", 256, (void *)"PrinterTask1", PRINTER_TASK_PRIO);
    Task_Create(printer_task, "PrinterTask2", 256, (void *)"PrinterTask2", PRINTER_TASK_PRIO);
    Task_Create(printer_task, "PrinterTask3", 256, (void *)"PrinterTask3", PRINTER_TASK_PRIO);

    /* --- 任务协作 (任务通知) --- */
    a_task_h = Task_Create(a_task, "TaskA", 64, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, "TaskB", 64, NULL, COLLABORATION_TASKS_PRIO);
    Task_Notify(a_task_h); // 启动第一个任务

    /* --- 动态任务管理 (创建与删除) --- */
    c_task_h = Task_Create(c_task, "TaskC_dynamic", 1024, NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO);

    /* --- 其他功能任务 --- */
    high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 128, NULL, INTERRUPT_TASK_PRIO);
}
```

#### 3\. 功能示例解析 (摘自 \`main.c\`)

##### 生产者-消费者模型 (消息队列)

`producer_task` 任务生产数据并发送到队列，而 `consumer_task` 任务从队列中等待并接收数据。

```c
typedef struct { uint32_t id; uint32_t data; } Product_t;
static QueueHandle_t product_queue;

void producer_task(void *param) {
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        LOG_I("生产者", "产品 ID %lu 已发送", product.id);
        Queue_Send(product_queue, &product, MS_TO_TICKS(100));
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void consumer_task(void *param) {
    Product_t received_product;
    while (1) {
        if (Queue_Receive(product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_I("消费者", "接收到产品 ID %lu", received_product.id);
        }
    }
}
```

##### 共享资源管理 (计数信号量)

模拟多个任务竞争有限的资源（如2台打印机）。任务必须先获取信号量才能“打印”，使用后释放信号量供其他任务使用。

```c
static SemaphoreHandle_t printer_semaphore; // 创建时设置为 Semaphore_Create(2, 2)

void printer_task(void *param) {
    const char *taskName = (const char *) param;
    while (1) {
        LOG_D(taskName, "正在等待打印机...");
        if (Semaphore_Take(printer_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_I(taskName, "获取到打印机, 开始打印 (耗时3秒)...");
            Task_Delay(MS_TO_TICKS(3000));
            LOG_I(taskName, "打印完成, 释放打印机.");
            Semaphore_Give(printer_semaphore);
        }
        Task_Delay(MS_TO_TICKS(500));
    }
}
```

##### 任务协作与同步 (任务通知)

`a_task` 和 `b_task` 通过互相发送通知来交替执行，展示了轻量级的任务间同步。

```c
void a_task(void *param) {
    while (1) {
        Task_Wait(); // 等待B的通知
        LOG_I("Task A", "被唤醒，开始工作...");
        Task_Delay(MS_TO_TICKS(1000));
        LOG_I("Task A", "工作完成，唤醒 Task B");
        Task_Notify(b_task_h);
    }
}

void b_task(void *param) {
    while (1) {
        Task_Wait(); // 等待A的通知
        LOG_I("Task B", "被唤醒，开始工作...");
        Task_Delay(MS_TO_TICKS(1000));
        LOG_I("Task B", "工作完成，唤醒 Task A");
        Task_Notify(a_task_h);
    }
}
```

##### 动态任务管理

`c_task` 运行5次后会调用 `Task_Delete(NULL)` 删除自身。`d_task` 负责监控 `c_task` 的状态，并在其被删除后重新创建它。

```c
void c_task(void *param) {
    uint16_t index = 0;
    while (1) {
        index++;
        LOG_D("Task C", "正在运行, 第 %d 次", index);
        if (index == 5) {
            LOG_W("Task C", "运行5次后删除自己.");
            c_task_h = NULL; // 清除全局句柄
            Task_Delete(NULL); // 删除自身
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void d_task(void *param) {
    while (1) {
        if (c_task_h == NULL) {
            LOG_I("Task D", "检测到Task C不存在, 准备重新创建...");
            Task_Delay(MS_TO_TICKS(3000));
            c_task_h = Task_Create(c_task, "TaskC_dynamic", 1024, NULL, COLLABORATION_TASKS_PRIO);
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}
```

##### 中断安全API (ISR From API)

硬件中断服务程序（ISR）通过调用 `FromISR` 版本的API（如 `Semaphore_GiveFromISR`）安全地与RTOS任务交互，唤醒正在等待的任务。

```c
// 中断服务程序
void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);
        int higherPriorityTaskWoken = 0;

        // 从ISR中释放信号量
        Semaphore_GiveFromISR(isr_semaphore, &higherPriorityTaskWoken);

        // 如果唤醒了更高优先级的任务，请求进行一次上下文切换
        MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
    }
}

// 等待中断信号量的任务
void isr_test_task(void *param) {
    LOG_I("ISR测试", "启动并等待信号量...");
    while (1) {
        if (Semaphore_Take(isr_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_I("ISR测试", "成功从按键中断获取信号量!");
        }
    }
}
```

## ⚙️ 调度机制

MyRTOS 的调度机制核心基于 Cortex-M4 的 SysTick 与 PendSV 中断，实现了优先级抢占与时间片轮转。

*   **SysTick 中断**: 作为系统心跳，周期性地触发，用于更新系统Tick计数和处理任务延时。
*   **就绪列表 (Ready List)**: 按优先级组织，存放所有可以立即运行的任务。
*   **调度器 (Scheduler)**: 在需要进行任务切换时（如SysTick中断、任务阻塞、任务唤醒），从就绪列表中选择优先级最高的任务作为下一个要运行的任务。对于同优先级的任务，采用时间片轮转（Round-Robin）方式调度。
*   **PendSV 中断**: 用于执行实际的上下文切换。它是一个可悬起、低优先级的中断，确保上下文切换不会在其他重要中断处理期间发生，从而降低了中断延迟。
    \-*   **空闲任务 (Idle Task)**: 当没有其他任何就绪任务时，CPU 执行空闲任务。通常可在此任务中执行低功耗处理或系统监控。

### 调度流程

![调度流程图](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/flow.svg)

## 📊 示例输出

以下为系统监视器 (Monitor) 服务的输出，展示了任务状态、CPU占用率、栈使用情况等信息：

![示例输出1](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log.png)

![示例输出2](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log2.png)

![示例输出3](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log3.png)

## 📜 参考

*   [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel)
*   [ChatGPT](https://chatgpt.com/)
*   ARM Cortex-M4 技术参考手册

## 👤 作者

A\_XiaoXiu
