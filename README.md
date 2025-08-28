MyRTOS README

# MyRTOS

一个基于 Cortex-M4 的多任务调度系统基本实现。参考 FreeRTOS 和 ChatGPT. 结构简单, 文件关系清晰, 测试用例直接可用,
适合作为个人学习过程中理解多任务调度的参考。



功能
=============
*   **分层架构**：内核、服务与平台实现完全解耦，提供统一API，支持不同平台轻松替换，实现高度可移植性与可维护性。
*   **多任务调度**：支持基于优先级的抢占式调度，以及同优先级时间片轮转。
*   **IPC机制**：
   *   **消息队列 (Queue)**：支持阻塞/非阻塞、带超时的任务间安全通信。
   *   **互斥锁 (Mutex)**：内置优先级继承协议，有效解决优先级反转。
   *   **递归互斥锁 (Recursive Mutex)**：支持同一任务递归加锁，简化复杂场景。
   *   **任务通知**：极低开销的任务间直接同步机制. (即将支持ISR中唤醒任务-FromISR操作)
*   **时间管理**：
   *   基于系统Tick的精准任务延时 (`Task_Delay`)。
   *   高效的软件定时器服务，支持创建大量的**周期性**与**一次性**定时器，不额外消耗硬件资源。
*   **动态内存管理**：内置高性能内存池，支持内存块的分配、释放与自动合并，有效防止内存碎片。
*   **完善的任务管理**：支持任务的动态创建与删除，可实时获取任务状态与优先级。
*   **系统监控与调试**：
   *   **实时监视器 (Monitor)**：通过串口实时显示任务状态、栈使用率、及**微秒级CPU运行时间**。
   *   **内存监控**：实时追踪堆内存使用情况，包括当前剩余与历史最低水位。
   *   **HardFault异常处理**：崩溃时自动打印详细的寄存器与故障信息，加速问题定位。
*   **异步日志服务**：独立任务处理日志，支持多级别(DEBUG, INFO, WARN, ERROR)输出，避免I/O操作阻塞业务任务。
*   **高度可配置**：所有功能、驱动和服务均可通过单一配置文件 (`MyRTOS_Config.h`) 进行裁剪和配置。

_\> **持续更新，不断完善中...**_

抽象架构
=============
![架构图](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/framework.svg)

<h2>目录结构</h2>
<pre>
MyRTOS-Project/
├── Firmware/ # 芯片官方库与核心文件
│ ├── CMSIS/ # Cortex-M 内核通用文件
│ └── GD32F4xx_standard_peripheral/ # GD32F4xx 标准外设库
├── <span style="background-color:yellow; font-weight:bold;">MyRTOS/</span> # MyRTOS 操作系统源码
│ ├── include/ # RTOS 对用户开放的所有API头文件 (统一接口层)
│ ├── kernel/ # 内核实现 (平台无关)
│ ├── services/ # 服务实现 (平台无关)
│ └── platform/ # 可替换的平台包
│ └── GD32F4xx/ # 针对 GD32F4xx 的具体实现
├── Project/ # Keil MDK 或其他 IDE 的工程文件
└── User/ # 用户应用代码
├── main.c # 应用入口，创建业务任务
└── MyRTOS_Config.h # RTOS 功能及平台配置文件 (用户唯一配置入口)
</pre>
<ul>
<li><strong>Firmware：</strong>存放芯片原厂商提供的底层驱动库，如 <strong>CMSIS</strong> 核心文件和 <strong>GD32标准外设库</strong>。这一层是RTOS运行的基础。</li>
<li><strong>MyRTOS：</strong>RTOS的全部源码，其内部结构清晰地体现了分层设计：
<ul>
<li><strong style="color:#0097a7;">include：</strong>RTOS的<strong>统一接口层</strong>。这是用户唯一需要包含的目录，它定义了RTOS提供的所有功能，包括内核API、服务API和标准化的硬件驱动API（如Timer）。</li>
<li><strong style="color:#1976d2;">kernel：</strong>RTOS的<strong>内核实现</strong>。包含任务调度、内存管理、IPC机制等纯软件逻辑，完全与硬件平台无关。</li>
<li><strong style="color:#f57c00;">services：</strong>RTOS的<strong>服务实现</strong>。包含日志、系统监视器等可选的功能模块，它们依赖内核API，但与硬件平台无关。</li>
<li><strong style="color:#c62828;">platform：</strong>RTOS的<strong>平台包</strong>，是连接上层软件与底层硬件的桥梁，是RTOS可移植性的关键。当前提供了一个针对 <strong>GD32F4xx</strong> 的实现包，其内部包含了：
<ul>
<li>CPU核心移植代码 (上下文切换、中断处理等)。</li>
<li>板级支持代码 (如调试串口 `PutChar` 的实现)。</li>
<li>标准硬件驱动的实现 (如 `MyRTOS_Driver_Timer` 的具体实现)。</li>
<li>统一的系统启动流程 (`MyRTOS_SystemInit`)。</li>
<li>一个供用户参考的 `MyRTOS_Config_Example.h` 配置文件模板。</li>
</ul>
</li>
</ul>
</li>
<li><strong>Project：</strong>存放IDE（如Keil MDK）的工程文件，负责组织代码、配置编译选项和链接。</li>
<li><strong>User：</strong>用户的最终应用代码。
<ul>
<li><strong>main.c：</strong>应用的入口点。现在它变得非常简洁，只需调用 `MyRTOS_SystemInit()`，创建初始任务，然后启动调度器。</li>
<li><strong>MyRTOS_Config.h：</strong><strong>用户唯一的配置中心</strong>。用户通过复制和修改平台包中的模板来创建此文件。它控制着RTOS的所有功能开关、硬件资源映射和平台定义，是整个项目的“指挥棒”。</li>
</ul>
</li>
</ul>


<ul>
    <li><strong>assets：</strong>示例资源文件，如图片、日志等辅助文件。</li>
    <li><strong>Firmware：</strong>硬件抽象层和外设库，支持 Cortex-M4 内核及 GD32F4 系列外设。</li>
    <li><strong>Libraries：</strong>通用库文件，提供工具函数或扩展外设功能。</li>
    <li><strong>MyRTOS：</strong>核心 RTOS 源码，包含以下子目录：
        <ul>
            <li><strong>core：</strong>内核层实现：任务调度、TCB管理、IPC机制、软件定时器。</li>
            <li><strong>portable：</strong>硬件抽象层接口实现（RTOS Port Layer），实现对不同 MCU 的适配。</li>
            <li><strong>services：</strong>服务与工具层：日志、系统监控等功能模块。</li>
            <li><strong>tool：</strong>辅助工具或调试模块。</li>
        </ul>
    </li>
    <li><strong>Project：</strong>Keil项目文件夹，用于编译和调试。</li>
    <li><strong>User：</strong>用户代码目录，用于具体应用示例和业务任务实现：
        <ul>
            <li><strong>main.c：</strong>应用入口，初始化硬件、RTOS服务及应用任务。</li>
            <li><strong>MyRTOS_Config.h：</strong>应用配置文件，通过编译控制 RTOS 各层功能。</li>
            <li><strong>tasks/：</strong>具体业务任务实现目录，如 a_task、producer_task 等。</li>
        </ul>
    </li>
</ul>



API 列表
=============

系统核心
----

* `void MyRTOS_Init(void)` — 初始化系统核心
* `void Task_StartScheduler(void)` — 启动任务调度器
* `uint64_t MyRTOS_GetTick(void)` — 获取当前系统时间 (ticks)

任务管理 (Task Management)
----------------------

* `TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority)` — 创建任务
* `int Task_Delete(TaskHandle_t task_h)` — 删除任务
* `void Task_Delay(uint32_t tick)` — 任务延时
* `int Task_Notify(TaskHandle_t task_h)` — 通知任务
* `void Task_Wait(void)` — 等待任务通知
* `TaskState_t Task_GetState(TaskHandle_t task_h)` — 获取任务状态
* `uint8_t Task_GetPriority(TaskHandle_t task_h)` — 获取任务优先级
* `TaskHandle_t Task_GetCurrentTaskHandle(void)` — 获取当前任务句柄

消息队列 (Queue Management)
-----------------------

* `QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize)` — 创建队列
* `void Queue_Delete(QueueHandle_t delQueue)` — 删除队列
* `int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks)` — 发送数据到队列
* `int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks)` — 从队列接收数据

定时器 (Timer Management)
----------------------

* `TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void* arg)` — 创建定时器
* `int Timer_Start(TimerHandle_t timer)` — 启动定时器
* `int Timer_Stop(TimerHandle_t timer)` — 停止定时器
* `int Timer_Delete(TimerHandle_t timer)` — 删除定时器

互斥锁 (Mutex Management)
----------------------

* `MutexHandle_t Mutex_Create(void)` — 创建互斥锁
* `void Mutex_Lock(MutexHandle_t mutex)` — 获取互斥锁
* `void Mutex_Unlock(MutexHandle_t mutex)` — 释放互斥锁
* `void Mutex_Lock_Recursive(MutexHandle_t mutex)` — 递归获取互斥锁
* `void Mutex_Unlock_Recursive(MutexHandle_t mutex)` — 递归释放互斥锁

时间转换宏
-----

* `MS_TO_TICKS(ms)` — 毫秒转 ticks
* `TICK_TO_MS(tick)` — ticks 转毫秒

核心宏
---

* `MY_RTOS_ENTER_CRITICAL(status_var)` — 进入临界区
* `MY_RTOS_EXIT_CRITICAL(status_var)` — 退出临界区
* `MY_RTOS_YIELD()` — 任务让出 CPU

快速开始
=========


配置文件示例
-------
MyRTOS 配置文件示例
-------------

    //
    // Created by XiaoXiu on 8/28/2025.
    //
    
    #ifndef MYRTOS_CONFIG_H
    #define MYRTOS_CONFIG_H
    
    //====================== 内核核心配置 ======================
    #define MY_RTOS_MAX_PRIORITIES              (16)      // 最大支持的优先级数量
    #define MY_RTOS_TICK_RATE_HZ                (1000)    // 系统Tick频率 (Hz)
    #define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // 最大延时ticks
    #define MY_RTOS_TASK_NAME_MAX_LEN           (16)       // 任务名称最大长度
    
    //====================== 内存管理配置 ======================
    #define RTOS_MEMORY_POOL_SIZE               (32 * 1024) // 内核使用的静态内存池大小 (bytes)
    #define HEAP_BYTE_ALIGNMENT                 (8)         // 内存对齐字节数
    
    //====================== 系统服务配置 ======================
    
    // --- 标准I/O (printf) 服务 ---
    #define MY_RTOS_USE_STDIO                   1 // 1: 启用printf重定向服务; 0: 禁用
    #if (MY_RTOS_USE_STDIO == 1)
    // 配置用于保护I/O输出的互斥锁等待时间
    #define MY_RTOS_IO_MUTEX_TIMEOUT_MS         100
    #endif
    
    //====================== 日志与I/O配置 ======================
    #define MY_RTOS_USE_LOG                     1 // 1: 启用日志和printf服务; 0: 禁用
    #if (MY_RTOS_USE_LOG == 1)
        // --- 日志级别定义 ---
        #define SYS_LOG_LEVEL_NONE              0 // 不打印任何日志
        #define SYS_LOG_LEVEL_ERROR             1 // 只打印错误
        #define SYS_LOG_LEVEL_WARN              2 // 打印错误和警告
        #define SYS_LOG_LEVEL_INFO              3 // 打印错误、警告和信息
        #define SYS_LOG_LEVEL_DEBUG             4 // 打印所有级别
    
        // 设置当前系统的日志级别
        #define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_INFO
    
        // --- 异步日志系统配置 ---
        #define SYS_LOG_QUEUE_LENGTH            30
        #define SYS_LOG_MAX_MSG_LENGTH          128
        #define SYS_LOG_TASK_PRIORITY           1
        #define SYS_LOG_TASK_STACK_SIZE         512
    #endif
    
    //====================== 运行时统计与监视器配置 ======================
    
    // --- 运行时统计功能 ---
    // 1: 开启运行时统计功能; 0: 关闭。这是监视器的基础，必须开启。
    #define MY_RTOS_GENERATE_RUN_TIME_STATS     1
    
    #if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    // --- 系统监视器 (Monitor) ---
    // 1: 启用系统监视器模块; 0: 禁用
    #define MY_RTOS_USE_MONITOR                 1
    
    #if (MY_RTOS_USE_MONITOR == 1)
    // 监视器任务的相关配置
    #define MY_RTOS_MONITOR_TASK_PRIORITY      (1)
    #define MY_RTOS_MONITOR_TASK_STACK_SIZE    (1024) // 需要较大栈来容纳缓冲区
    #define MY_RTOS_MONITOR_TASK_PERIOD_MS     (500) // 监视器刷新周期
    
    // 监视器用于格式化输出的缓冲区大小
    #define MY_RTOS_MONITOR_BUFFER_SIZE        (2048)
    #endif
    #endif
    
    #endif // MYRTOS_CONFIG_H

任务创建与调度
-------

    TaskHandle_t task1, task2;
    
    void task1_func(void *param) {
        while (1) {
            printf("任务1运行中 (低优先级)\n");
            Task_Delay(1000);
        }
    }
    
    void task2_func(void *param) {
        while (1) {
            printf("任务2运行中 (高优先级)\n");
            Task_Delay(2000);
        }
    }
    
    void main_func(void) {
        task1 = Task_Create(task1_func, 256, NULL, 1); // 低优先级
        task2 = Task_Create(task2_func, 256, NULL, 3); // 高优先级
        Task_StartScheduler();
    }

互斥锁
---

    MutexHandle_t lock;
    
    void task_with_lock(void *param) {
        while (1) {
            Mutex_Lock(lock);
            printf("任务获取锁，安全访问共享资源\n");
            Task_Delay(500);
            Mutex_Unlock(lock);
            Task_Delay(500);
        }
    }
    
    void sys_init() {
        lock = Mutex_Create();
    }

递归锁
---

    MutexHandle_t recursive_lock;
    
    void recursive_task(void *param) {
        while (1) {
            Mutex_Lock_Recursive(recursive_lock);
            printf("第一次加锁\n");
            Mutex_Lock_Recursive(recursive_lock);
            printf("第二次加锁\n");
            Mutex_Unlock_Recursive(recursive_lock);
            Mutex_Unlock_Recursive(recursive_lock);
            Task_Delay(1000);
        }
    }
    
    void sys_init() {
        recursive_lock = Mutex_Create();
    }

消息队列
----

    typedef struct { int id; } Msg_t;
    QueueHandle_t msg_queue;
    
    void producer(void *param) {
        Msg_t msg = {0};
        while (1) {
            msg.id++;
            Queue_Send(msg_queue, &msg, MY_RTOS_MAX_DELAY);
            printf("生产者发送: id=%d\n", msg.id);
            Task_Delay(1000);
        }
    }
    
    void consumer(void *param) {
        Msg_t msg;
        while (1) {
            if (Queue_Receive(msg_queue, &msg, MY_RTOS_MAX_DELAY)) {
                printf("消费者接收: id=%d\n", msg.id);
            }
        }
    }
    
    void sys_init() {
        msg_queue = Queue_Create(3, sizeof(Msg_t));
    }

任务通知
----

    TaskHandle_t t1, t2;
    
    void t1_func(void *param) {
        while (1) {
            printf("任务1等待通知\n");
            Task_Wait();
            printf("任务1收到通知\n");
        }
    }
    
    void t2_func(void *param) {
        while (1) {
            Task_Delay(2000);
            printf("任务2发送通知给任务1\n");
            Task_Notify(t1);
        }
    }

任务自删除
-----

    TaskHandle_t t3;
    
    void t3_func(void *param) {
        printf("任务3启动，立即删除自身\n");
        Task_Delete(t3);
    }

定时器触发任务
-------

    TimerHandle_t timer;
    
    void timer_cb(TimerHandle_t t) {
        printf("定时器回调触发任务执行\n");
    }
    
    void sys_init() {
        timer = Timer_Create(1000, 1000, timer_cb, NULL); // 周期1秒
        Timer_Start(timer);
    }

中断唤醒任务
------

    TaskHandle_t irq_task;
    
    void irq_task_func(void *param) {
        while (1) {
            Task_Wait(); // 等待外部事件唤醒
            printf("外部中断唤醒任务执行\n");
        }
    }
    
    void EXTI0_IRQHandler(void) {
        if (irq_task != NULL) {
            Task_Notify(irq_task); // 通过通知唤醒任务
        }
    }

优先级抢占
-------

    TaskHandle_t low, high;
    
    void low_func(void *param) {
        while (1) {
            printf("低优先级任务运行中\n");
            Task_Delay(1000);
        }
    }
    
    void high_func(void *param) {
        while (1) {
            Task_Delay(3000);
            printf("高优先级任务抢占执行\n");
        }
    }
    
    void main_func(void) {
        low = Task_Create(low_func, 256, NULL, 1); // 低优先级
        high = Task_Create(high_func, 256, NULL, 5); // 高优先级
        Task_StartScheduler();
    }

调度机制
=============


MyRTOS 的调度机制核心基于 Cortex-M4 的 SysTick 与 PendSV 中断，实现了优先级抢占与时间片轮转，并支持延时任务、互斥锁和任务通知。

### 机制说明

* **SysTick 中断：**每 1ms 触发一次，用于系统心跳和任务延时计数。
* **延时管理：**检查所有延时任务，到期后移入就绪队列（Ready List）。
* **调度器 Scheduler：**根据任务优先级选择最高优先级 READY 任务执行，同优先级任务采用时间片轮转。
* **PendSV 上下文切换：**保存当前任务的寄存器状态，恢复下一个任务上下文，实现任务切换。
* **任务抢占：**高优先级任务可以随时抢占低优先级任务执行，保证实时性。
* **任务等待与互斥锁：**任务可能因为等待通知或被互斥锁阻塞而挂起，待条件满足后重新加入就绪队列。
* **空闲任务：**当没有 READY 任务时，CPU 执行空闲任务 Idle，通常用于节能或系统监控。

通过以上机制，MyRTOS 可以实现多任务优先级抢占、时间片轮转、延时任务、互斥资源保护和任务通知等功能。

### 调度流程

![流程图](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/flow.svg)

## 示例输出

![示例输出](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log.png)
![示例输出2](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log2.png)
![示例输出3](https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log3.png)

## 参考

* [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel)
* ARM Cortex-M4

## 作者

A\_XiaoXiu