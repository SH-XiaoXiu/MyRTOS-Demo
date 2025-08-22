MyRTOS
======

一个简易的基于 Cortex-M4 的多任务调度系统。参考 FreeRTOS, ChatGPT。  
仅供个人学习过程中，作为理解多任务调度的参考。

特性
--

*   轮询调度（无优先级）
*   基于 SysTick 的任务延时
*   PendSV 上下文切换
*   任务通知机制
*   互斥锁支持
*   Idle 任务（低功耗 WFI）
*   HardFault 基础调试输出

目录结构
----


    MyRTOS/
    ├── MyRTOS.c       // 内核实现
    ├── MyRTOS.h       // API 定义
    ├── main.c         // 示例任务
    ├── lib_usart0.*   // 串口调试
    └── gd32f4xx_*     // 厂商库

快速开始
----

    C
    sys_config();  
    Task_Create(A_TASK, a_task, NULL);
    Task_Create(B_TASK, b_task, NULL);
    Task_Create(C_TASK, c_task, NULL);
    Task_StartScheduler();  // 不会返回


示例任务
----

    C
    void a_task(void *param) {
        while (1) {
            printf("任务A运行中\n");
            Task_Delay(1000);
        }
    }


API 列表
------

*   `Task_Create(id, func, param)` 创建任务
*   `Task_StartScheduler()` 启动调度器
*   `Task_Delay(tick)` 延时
*   `Task_Wait()` 阻塞等待通知
*   `Task_Notify(id)` 唤醒任务
*   `Mutex_Init/Lock/Unlock` 互斥锁

调度机制
----

1.  SysTick 每 1ms 更新延时并触发 PendSV
2.  PendSV 保存当前任务寄存器并恢复下一个
3.  按任务 ID 顺序轮询选择 READY 任务

限制
--

*   最大任务数 `MAX_TASKS = 8`
*   固定栈大小 `STACK_SIZE = 256`
*   无任务删除
*   无优先级调度
*   无时间片抢占

示例输出
----
```text
=========   =========================
|        RTOS Demo
|  Author: XiaoXiu
|  Version: 0.0
|  MCU: GD32
==================================
System Starting...
任务 C 启动
任务 C 唤醒 任务 A
任务 A 启动
任务 B 启动

任务 C 正在运行, 第 0 次
A 任务正在运行, 第 0 次
A 任务正在运行, 第 1 次
A 任务正在运行, 第 2 次
A 任务正在运行, 第 3 次
A 任务正在运行, 第 4 次
任务 A 唤醒 任务 B, 并开始等待 任务 B 的唤醒

B 任务正在运行, 第 0 次
B 任务正在运行, 第 1 次
B 任务正在运行, 第 2 次

任务 C 正在运行, 第 1 次
任务 C 正在运行, 第 2 次
...
```




参考
--
*   [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel)
*   ARM Cortex-M4

作者
--

A\_XiaoXiu