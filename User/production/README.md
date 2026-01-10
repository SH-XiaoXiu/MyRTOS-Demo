# Production Mode

生产模式，符合架构规范的正确使用方式。

## 特点

1. **严格的层级分离**
   - 用户代码**禁止**直接使用 `Task_Create`
   - 所有用户逻辑必须是 **Process**

2. **Init 进程架构**
   - PID 1：init 进程（系统进程）
   - PID 2+：Shell、用户程序

3. **完整的进程生命周期**
   - 进程创建、运行、退出
   - 信号处理（Ctrl+C/Z/B）
   - 前台/后台管理

## 启动流程

```
内核启动
  ↓
Platform_Init()
  ↓
启动调度器
  ↓
创建 init 进程 (PID 1)
  ↓
init 启动 Shell 进程 (PID 2)
  ↓
用户通过 Shell 运行程序 (PID 3+)
```

## 对比 Demo 模式

| 特性 | Demo 模式 | Production 模式 |
|------|----------|----------------|
| Task | ✅ 允许 | ❌ 禁止 |
| Process | ✅ 可用 | ✅ 必须 |
| Shell | Task | Process |
| 层级暴露 | 全部 | 仅 Ring 3 |
