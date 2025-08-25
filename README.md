<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <title>MyRTOS README</title>
</head>
<body>
  <h1>MyRTOS</h1>

  <p>一个简易的基于 Cortex-M4 的多任务调度系统。参考 FreeRTOS 和 ChatGPT。  
  仅供个人学习过程中，作为理解多任务调度的参考。</p>

<h2>特性</h2>
  <ul>
    <li>动态内存管理（内存池实现）</li>
    <li>支持优先级调度与时间片轮转</li>
    <li>基于 SysTick 的任务延时</li>
    <li>PendSV 上下文切换</li>
    <li>任务创建与删除</li>
    <li>任务通知机制</li>
    <li>信号量与互斥锁支持</li>
    <li>消息队列（任务间通信）</li>
    <li>Idle 任务（低功耗 WFI）</li>
    <li>HardFault 基础调试输出</li>
  </ul>

<h2>目录结构</h2>
  <pre>
MyRTOS/
├── core/          // 内核源码
├── drivers/       // 硬件抽象与外设驱动
├── examples/      // 示例任务
├── include/       // 公共 API 头文件
├── linker/        // 链接脚本
├── lib_usart0.*   // 串口调试
├── gd32f4xx_*     // 厂商库
└── main.c         // 示例入口
  </pre>

<h2>快速开始</h2>
  <pre><code class="language-c">
sys_config();
Task_Create(A_TASK, a_task, NULL);
Task_Create(B_TASK, b_task, NULL);
Task_Create(C_TASK, c_task, NULL);
Task_StartScheduler(); 
  </code></pre>

<h2>示例任务</h2>
  <pre><code class="language-c">
void a_task(void *param) {
    while (1) {
        printf("任务A运行中\n");
        Task_Delay(1000);
    }
}
  </code></pre>

<h2>API 列表</h2>
  <ul>
    <li><code>Task_Create(func, param)</code> 创建任务</li>
    <li><code>Task_Delete(task)</code> 删除任务</li>
    <li><code>Task_StartScheduler()</code> 启动调度器</li>
    <li><code>Task_Delay(tick)</code> 延时</li>
    <li><code>Task_Wait()</code> 阻塞等待通知</li>
    <li><code>Task_Notify(id)</code> 唤醒任务</li>
    <li><code>Semaphore_Init / Semaphore_Take / Semaphore_Give</code> 信号量</li>
    <li><code>Mutex_Init / Mutex_Lock / Mutex_Unlock</code> 互斥锁</li>
    <li><code>Queue_Create / Queue_Send / Queue_Recv</code> 消息队列</li>
    <li><code>rtos_malloc / rtos_free</code> 动态内存分配</li>
  </ul>

<h2>调度机制</h2>
  <ol>
    <li>SysTick 每 1ms 更新延时并触发 PendSV</li>
    <li>PendSV 保存当前任务寄存器并恢复下一个</li>
    <li>调度器根据优先级选择最高优先 READY 任务；同优先级采用时间片轮询</li>
  </ol>

<h2>限制</h2>
  <ul>
    <li>固定栈大小 <code>STACK_SIZE = 256</code></li>
    <li>最大任务数取决于内存池大小</li>
  </ul>

<h2>示例输出</h2>
  <p><img src="https://gitee.com/sh-xiaoxiu/my-rtos-demo/raw/main/assets/usart_log.png" alt="示例输出"></p>

<h2>参考</h2>
  <ul>
    <li><a href="https://github.com/FreeRTOS/FreeRTOS-Kernel">FreeRTOS</a></li>
    <li>ARM Cortex-M4</li>
  </ul>

<h2>作者</h2>
  <p>A_XiaoXiu</p>
</body>
</html>
