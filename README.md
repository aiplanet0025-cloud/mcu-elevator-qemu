***

# Elevator OS - 基于 FreeRTOS & QEMU 的工业级电梯控制模拟系统

`Elevator OS` 是一个运行在 ARM Cortex-M3 (模拟芯片：Stellaris LM3S6965) 平台上的、高可靠性工业级电梯控制模拟系统。项目利用 **QEMU 虚拟机** 在云端 (GitHub Codespaces) 构建起完整的软硬件仿真环境。

本项目**拒绝**传统的面向过程“面条式”裸机代码开发，而是严格遵循现代嵌入式软件工程规范，在系统高内聚、低耦合、线程安全、内存安全及高确定性等方面进行了深度工程化实践，达到满足工业安全标准（如 IEC 61508 / MISRA-C）的原型设计高度。

---

## 🌟 核心技术亮点与工程设计优势

### 1. 严格的三层解耦架构 (Layered Software Architecture)
系统在软件设计上实现了严格的分层，任何一层的变动都不会影响到其他层，具备极佳的平台移植性：
*   **BSP / HAL 层 (板级支持与硬件抽象)**：封装寄存器级操作（如 `bsp_uart.c`），向上仅暴露标准的、平台无关的底层接口。
*   **OS 任务与同步层 (OS & Sync Layer)**：利用 **FreeRTOS Queue (消息队列)** 实现多任务异步非阻塞通信；利用 **Mutex (互斥锁)** 构建了线程安全的调试日志机制。
*   **APP 业务逻辑层 (Business Logic)**：电梯的核心状态机 (`elevator_fsm.c`) 为 **100% 纯 C 语言编写**，不依赖 FreeRTOS 的任何 API，也不包含任何硬件寄存器头文件。这使得该业务逻辑可以直接在 PC 端运行 Google Test 等框架实施高覆盖率的单元测试。

### 2. 100% 静态内存分配 (Safety-Hardened Design)
在汽车电子、航空航天及生命安全相关的工业嵌入式设备中，动态内存分配（如 `malloc`、动态创建 Task）因其可能产生**内存碎片**并在长期运行中引发突发性死机，是严厉禁止的。
*   本系统将 **所有的应用任务**（如电梯控制、命令行交互）以及 **所有的进程通信机制**（如消息队列、互斥锁）全部转型为**静态内存分配 (`xTaskCreateStatic` 和 `xQueueCreateStatic`)**。
*   静态堆栈和控制块在编译阶段即在系统的 `.bss` 段锁定。只要固件编译通过，在物理上就**绝对不会发生“因堆内存耗尽/碎片化而死机”**的系统崩溃事故，确保了系统的高确定性。

### 3. 高度防护的内存边界管理 (MSP / PSP 隔离)
针对 ARM Cortex-M 的双堆栈指针机制（MSP/PSP）进行了严密的防御性设计：
*   **系统启动期 (MSP)**：重构了底层的栈大小参数（`-DSTACK_SIZE=2048`），彻底解决了在 `main()` 函数中由于标准 C 库 `vsnprintf` 深度压栈导致的启动期主栈溢出问题。
*   **任务运行期 (PSP)**：通过对 `vsnprintf` 最大栈开销的科学定量测算，将各静态任务的局部栈分配优化为 `400` 字 (1600 字节)，在仅有 64 KB SRAM 的严苛芯片空间限制下，实现了安全性与空间利用率的最佳平衡。

### 4. 线程安全的实时诊断终端 (Interactive CLI Console)
设计并实现了一个类似 Linux 终端的异步交互式命令行：
*   **硬件回显 (Echo) 与删除支持**：支持字符输入回显及 VT100 退格转义（Backspace）物理擦除。
*   **非阻塞指令解析**：通过非阻塞 UART 读取配合 FreeRTOS 队列，用户输入 `status` 即可异步查询正在运行中的电梯状态（如位置、方向、电机状态），完美展现了实时操作系统的并发多任务设计优势。

---

## 📁 项目目录结构

```text
/workspaces/mcu-elevator-qemu
├── CMakeLists.txt             # 顶层 CMake 配置文件，负责交叉编译与标志配置
├── standalone.ld              # 针对 QEMU 模拟芯片 (LM3S6965) 的链接脚本
├── src/
│   ├── main.c                 # 系统入口，初始化硬件、静态 OS 资源并开启调度器
│   ├── FreeRTOSConfig.h       # FreeRTOS 配置文件，开启互斥锁、静态分配及断言重定向
│   ├── bsp/                   # 底层驱动层
│   │   ├── startup.c          # 芯片启动汇编转 C 的文件，定义向量表与 ResetISR
│   │   ├── bsp_uart.h         # 串口抽象头文件
│   │   └── bsp_uart.c         # 串口寄存器驱动
│   ├── app/                   # 纯 C 业务逻辑层
│   │   ├── elevator_fsm.h     # 电梯状态机头文件
│   │   └── elevator_fsm.c     # 状态转移逻辑
│   └── os_tasks/              # 系统任务与互斥保护层
│       ├── logger.h           # 线程安全日志头文件
│       ├── logger.c           # 互斥锁保护的日志输出
│       ├── task_cli.h         # CLI 交互任务头文件
│       └── task_cli.c         # 诊断控制台字符流解析
```

---

## 🛠️ 运行环境搭建 (Prerequisites)

本项目在基于 Ubuntu/Debian 的 Linux 容器（如 GitHub Codespaces）中运行，所需工具链如下：

```bash
# 更新源并安装 ARM 交叉编译器、QEMU 系统仿真器和构建工具
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi gdb-multiarch qemu-system-arm cmake make git
```

### 拉取 Git 外部子模块（必执行）
项目使用了 FreeRTOS 官方仓库，在首次克隆后需初始化内核源码子模块：
```bash
git submodule update --init --recursive
```

---

## ⚙️ 项目编译指南

本项目使用 **CMake** 作为构建系统，并强制开启了针对嵌入式裸机的工具链配置，并过滤掉了第三方标准库中的 `.eh_frame` 调试段，解决内存段重合冲突。

```bash
# 1. 创建并进入 build 目录
mkdir -p build && cd build

# 2. 生成 Makefile
cmake ../

# 3. 编译（将生成 RTOSDemo.elf 固件）
make
```

---

## 🚀 QEMU 仿真与人机交互

在 `build` 目录下，执行以下命令在 QEMU 的 Cortex-M3 虚拟机器上启动该固件：

```bash
qemu-system-arm -M lm3s6965evb -kernel RTOSDemo -nographic
```

### 💻 交互命令手册

系统正常启动后，终端将输出：
```text
==============================================
  Elevator Diagnostics Console (CLI) Ready
  Type 'help' to see available commands.
==============================================
elevator> 
```

你可以在控制台输入以下命令与系统实时交互：

*   **`help`**：查看支持的指令菜单。
*   **`status`**：即时跨任务查询电梯状态。
*   **`call <1-3>`**：呼叫电梯。例如输入 `call 3` 并按下回车，电梯状态机即被唤醒并开始模拟物理位移，此时你可以尝试输入 `status` 进行并发查询：

```text
elevator> call 3
[FSM] Call Accepted! Target set to 3. Motor: STARTING UP...

[CLI] Success: Event 'call 3' queued.
elevator> status

[Status] Curr Floor: 1 | Target Floor: 3 | Motor State: MOVING UP
elevator>   >> Elevator arrived at floor 2.
  >> Elevator arrived at floor 3.
[FSM] Door: OPENING at floor 3.
[FSM] Door: CLOSING...
[FSM] Door: CLOSED. Elevator is now IDLE.
```

### 🚪 如何退出模拟器
由于 QEMU 接管了终端，若要退出模拟器回到 Linux 控制台，请在终端中按下：
*   同时按下 **`Ctrl + A`**，松开后，按下字母 **`X`** 键。
