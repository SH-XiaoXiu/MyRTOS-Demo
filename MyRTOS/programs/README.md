# MyRTOS Programs

用户态程序（Ring 3），所有程序都以 Process 形式运行。

## 结构

```
programs/
├─ init/           # Init 进程 (PID 1)
├─ shell/          # Shell 程序 (使用 ShellCore 库)
└─ demos/          # 示例程序
    ├─ looper/
    ├─ echo/
    └─ hello/
```

## Init 进程 (PID 1)

特殊的系统进程，负责：
1. 启动 Shell 或其他系统服务
2. 监控关键进程，崩溃后重启
3. 处理孤儿进程

## Shell 程序

普通的用户程序，但提供交互界面：
- 使用 ShellCore 库
- 作为 Process 运行（非特权）
- 可以被 kill，由 init 重启

## 与 Task 的区别

- ❌ **不要** 使用 `Task_Create` 创建用户程序
- ✅ **应该** 使用 `Process_Create` 或 `Process_RunProgram`
- Ring 0 (Task) 只给内核和系统服务使用
- Ring 3 (Process) 是用户程序的正确抽象
