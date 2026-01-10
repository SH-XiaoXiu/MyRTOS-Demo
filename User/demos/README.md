# Demo Mode

演示模式，展示 MyRTOS 所有层级的功能。

## 目的

教学和功能展示，**不代表实际使用方式**。

## 内容

### kernel_demos/
演示内核原语（Ring 0）：
- Task 创建和调度
- Mutex、Semaphore、Queue
- 软件定时器

### service_demos/
演示系统服务（Ring 1）：
- Process 管理
- Log 系统
- VTS 虚拟终端

## 注意

⚠️ Demo 模式中直接使用 `Task_Create` 等内核 API
⚠️ 这在 Production 模式中是**禁止**的
⚠️ Production 模式只能使用 Process API
