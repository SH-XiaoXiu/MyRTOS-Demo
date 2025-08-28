MyRTOS README

# MyRTOS

一个基于 Cortex-M4 的多任务调度系统基本实现。参考 FreeRTOS 和 ChatGPT. 结构很简单, 一个头文件一个源文件,
适合作为个人学习过程中理解多任务调度的参考。



功能
------------------------------

*   > 解决了优先级反转问题.
* **多任务调度**：支持基于优先级的抢占调度以及同优先级时间片轮转，确保高优先级任务及时执行。
* **可重入互斥锁（Recursive Mutex）**：支持递归锁和普通互斥锁，解决共享资源访问冲突.
* **任务延时与定时器**：
    * 基于 SysTick 的任务延时机制
    * 周期性和一次性定时器支持
    * 可触发任务或回调函数
* **任务通知机制**：轻量级事件通知，任务可等待或接收通知以触发执行，支持中断唤醒。
* **消息队列（Queue）**：支持多任务间通信，队列长度和数据类型可自定义，阻塞与非阻塞接收模式。
* **动态内存管理**：内存池实现，支持任务栈、消息队列等动态资源分配，避免碎片化。
* **任务管理**：
    * 动态创建和删除任务
    * 获取任务状态与优先级
    * 支持空闲任务与系统心跳
* **中断与调度集成**：
    * 支持外部中断唤醒任务
    * PendSV 上下文切换，保证任务切换安全高效
* **系统可调试性**：
    * 支持 HardFault 基础调试输出
    * 调试串口输出示例
* **轻量简洁**：源码结构简单，仅一个头文件和一个源文件，方便学习和定制。

<h2>目录结构</h2>
<pre>
MyRTOS/
├── assets/             # 示例资源文件，如图片、日志
├── Firmware/           # 硬件抽象层与外设库
│   ├── CMSIS/          # Cortex-M4 核心相关头文件与源码
│   ├── GD32F4xx_standard_peripheral/  # 标准外设驱动（GPIO、USART、Timer 等）
│   └── GD32F4xx_usb_library/          # USB 设备与主机库
├── Libraries/          # 通用库文件，扩展外设功能
├── <span style="background-color:yellow; font-weight:bold;">MyRTOS/</span>   # 核心 RTOS 源码
├── Project/            # Keil工程
│   ├── Listings/       # 汇编/编译列表文件
│   └── Objects/        # 目标文件
└── User/               # 用户应用示例代码
</pre>
<ul>
    <li><strong>Firmware：</strong>硬件抽象层和外设库，支持 Cortex-M4 核心及 GD32F4 系列外设。</li>
    <li><strong>Libraries：</strong>通用库文件，提供工具函数或扩展外设功能。</li>
    <li><strong>MyRTOS：</strong>核心 RTOS 源码，包括任务调度、消息队列、互斥锁、定时器等实现。</li>
    <li><strong>Project：</strong>Keil项目文件夹</li>
    <li><strong>User：</strong>用户代码目录，用于具体应用示例。</li>
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

## 参考

* [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel)
* ARM Cortex-M4

## 作者

A\_XiaoXiu