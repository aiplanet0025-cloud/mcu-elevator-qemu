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
*   本系统将 **所有的应用任务**（如电梯控制、命令行交互）以及 **所有的进程通信机制**（如消息队列、互斥锁）全部分配为**静态内存分配 (`xTaskCreateStatic`、`xQueueCreateStatic` 和 `xSemaphoreCreateMutexStatic`)**。
*   静态堆栈和控制块在编译阶段即在系统的 `.bss` 段锁定；FreeRTOS 配置关闭动态分配（`configSUPPORT_DYNAMIC_ALLOCATION=0`），构建系统也不链接 `heap_*.c` 内存管理实现。只要固件编译通过，在物理上就**绝对不会发生“因 FreeRTOS 堆内存耗尽/碎片化而死机”**的系统崩溃事故，确保了系统的高确定性。

### 3. 高度防护的内存边界管理 (MSP / PSP 隔离)
针对 ARM Cortex-M 的双堆栈指针机制（MSP/PSP）进行了严密的防御性设计：
*   **系统启动期 (MSP)**：重构了底层的栈大小参数（`-DSTACK_SIZE=2048`），彻底解决了在 `main()` 函数中由于标准 C 库 `vsnprintf` 深度压栈导致的启动期主栈溢出问题。
*   **任务运行期 (PSP)**：通过对 `vsnprintf` 最大栈开销的科学定量测算，将各静态任务的局部栈分配限制在 `400` 字 (1600 字节)，在仅有 64 KB SRAM 的严苛芯片空间限制下，实现了安全性与空间利用率的最佳平衡。

### 4. 线程安全的实时诊断终端 (Interactive CLI Console)
设计并实现了一个类似 Linux 终端的异步交互式命令行：
*   **字符回显 (Echo) 与删除支持**：支持字符输入回显及 VT100 退格转义（Backspace）物理擦除。
*   **非阻塞指令解析**：通过非阻塞 UART 读取配合 FreeRTOS 队列，用户输入 `status` 即可异步查询正在运行中的电梯状态（如位置、方向、电机状态），完美展现了实时操作系统的并发多任务设计优势。

---

## 📁 项目目录结构

```text
/workspaces/mcu-elevator-qemu
├── CMakeLists.txt             # 顶层 CMake 项目配置文件
├── cmake/
│   └── toolchain-arm-none-eabi.cmake  # ARM 裸机交叉编译工具链配置
├── standalone.ld              # 针对 QEMU 模拟芯片 (LM3S6965) 的链接脚本
├── setup.sh                   # 依赖拉取与本地环境检查脚本（不会重写业务源码）
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

---

## 🚀 依赖初始化 (Dependency Setup)

`setup.sh` 只负责本地环境检查和外部依赖拉取：它会下载或更新官方 FreeRTOS 仓库，并初始化 `FreeRTOS/Source` 子模块。脚本不会重写 `src/`、`cmake/`、`CMakeLists.txt` 或其它业务源码，避免开发者本地改动被初始化流程覆盖。

在项目根目录下直接运行：

```bash
# 1. 赋予脚本执行权限
chmod +x setup.sh

# 2. 拉取/更新 FreeRTOS 依赖
./setup.sh
```

### 📋 脚本执行的自动化流程：
1. **工具检查**：要求 `git` 存在；对 `cmake`、`make`、`arm-none-eabi-gcc`、`qemu-system-arm` 等构建/仿真工具给出缺失警告，但不阻止依赖拉取。
2. **依赖检索**：如果 `FreeRTOS/` 不存在，则克隆官方 FreeRTOS 仓库并拉取子模块。
3. **安全更新**：如果 `FreeRTOS/` 已经是 git checkout，则执行 fast-forward 更新和子模块初始化。
4. **覆盖保护**：如果 `FreeRTOS/` 是非空、非 git 目录，脚本会先询问是否覆盖；在 CI 等非交互环境中会拒绝覆盖，除非显式传入 `./setup.sh --force`。
5. **依赖校验**：确认 `FreeRTOS/FreeRTOS/Source/tasks.c` 存在，否则给出明确错误并终止。

---

## ⚙️ 项目编译指南

完成依赖初始化后，即可使用 **CMake** 进行编译。首次配置时必须显式传入 ARM 裸机工具链文件，确保 CMake 在 `project()` 语言检测前就选择 `arm-none-eabi-gcc`，而不是宿主机编译器。CMake 会在配置阶段检查 `FreeRTOS/FreeRTOS/Source/tasks.c`，依赖缺失时会明确提示运行 `./setup.sh` 或通过 `-DFREERTOS_ROOT=/path/to/FreeRTOS` 指定已有 checkout：

```bash
# 1. 使用 ARM 裸机工具链文件配置 build 目录
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake

# 2. 编译（将生成最终的 RTOSDemo 固件）
cmake --build build
```

---

## 🎮 QEMU 仿真与人机交互

在 `build` 目录下，执行以下命令在 QEMU 的 Cortex-M3 虚拟机器上启动固件：

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

你可以在控制台输入以下指令进行实时设备诊断与呼叫交互：

*   **`help`**：查看支持的指令菜单。
*   **`status`**：即时跨任务查询电梯物理状态。
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
```
