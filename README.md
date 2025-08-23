<h1>MyRTOS</h1>

<p>一个简易的基于 Cortex-M4 的多任务调度系统。参考 FreeRTOS 和 ChatGPT。  
仅供个人学习过程中，作为理解多任务调度的参考。</p>

<h2>特性</h2>
<ul>
  <li>动态内存管理 (简单内存池)</li>
  <li>轮询调度（无优先级）</li>
  <li>基于 SysTick 的任务延时</li>
  <li>PendSV 上下文切换</li>
  <li>任务创建与删除</li>
  <li>任务通知机制</li>
  <li>互斥锁支持</li>
  <li>Idle 任务（低功耗 WFI）</li>
  <li>HardFault 基础调试输出</li>
</ul>

<h2>目录结构</h2>
<pre>
MyRTOS/
├── MyRTOS.c       // 内核实现
├── MyRTOS.h       // API 定义
├── main.c         // 示例任务
├── lib_usart0.*   // 串口调试
└── gd32f4xx_*     // 厂商库
</pre>

<h2>快速开始</h2>
<pre><code class="language-c">
sys_config();
Task_Create(A_TASK, a_task, NULL);
Task_Create(B_TASK, b_task, NULL);
Task_Create(C_TASK, c_task, NULL);
Task_StartScheduler();  // 不会返回
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
  <li><code>Mutex_Init / Mutex_Lock / Mutex_Unlock</code> 互斥锁</li>
  <li><code>rtos_malloc / rtos_free</code> 动态内存分配</li>
</ul>

<h2>调度机制</h2>
<ol>
  <li>SysTick 每 1ms 更新延时并触发 PendSV</li>
  <li>PendSV 保存当前任务寄存器并恢复下一个</li>
  <li>按任务链表顺序轮询选择 READY 任务</li>
</ol>

<h2>限制</h2>
<ul>
  <li>固定栈大小 <code>STACK_SIZE = 256</code></li>
  <li>最大任务数取决于内存池大小</li>
  <li>无优先级调度</li>
  <li>无时间片抢占</li>
</ul>

<h2>示例输出</h2>
<pre><code>
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
</code></pre>

<h2>参考</h2>
<ul>
  <li><a href="https://github.com/FreeRTOS/FreeRTOS-Kernel">FreeRTOS</a></li>
  <li>ARM Cortex-M4</li>
</ul>

<h2>作者</h2>
<p>A_XiaoXiu</p>
