***

# Elevator OS - An Industrial-Grade Elevator Control Simulation System Based on FreeRTOS & QEMU

`Elevator OS` is a high-reliability, industrial-grade elevator control simulation system that runs on an ARM Cortex-M3 platform (simulated chip: Stellaris LM3S6965). The project uses the **QEMU virtual machine** to build a complete software/hardware simulation environment in the cloud, such as GitHub Codespaces.

This project **rejects** traditional procedural, "spaghetti-style" bare-metal development. Instead, it strictly follows modern embedded software engineering practices and applies deep engineering discipline around high cohesion, low coupling, thread safety, memory safety, and high determinism. The result is a prototype designed toward the expectations of industrial safety standards such as IEC 61508 and MISRA-C.

---

## 🌟 Core Technical Highlights and Engineering Advantages

### 1. Strict Three-Layer Decoupled Architecture (Layered Software Architecture)

The system implements strict software layering. Changes in any one layer do not affect the others, which provides excellent platform portability:

* **BSP / HAL Layer (Board Support and Hardware Abstraction)**: Encapsulates register-level operations, such as `bsp_uart.c`, and exposes only standard, platform-independent low-level interfaces upward.
* **OS Task and Synchronization Layer (OS & Sync Layer)**: Uses **FreeRTOS Queue** objects for asynchronous, non-blocking communication between tasks, and uses **Mutex** objects to build a thread-safe debug logging mechanism.
* **APP Business Logic Layer (Business Logic)**: The elevator core state machine (`elevator_fsm.c`) is written in **100% pure C**, without any dependency on FreeRTOS APIs and without any hardware register header files. This allows the business logic to run directly on a PC with frameworks such as Google Test for high-coverage unit testing.

### 2. 100% Static Memory Allocation (Safety-Hardened Design)

In automotive electronics, aerospace, and life-safety-related industrial embedded devices, dynamic memory allocation, such as `malloc` or dynamically created tasks, is strictly prohibited because it can cause **memory fragmentation** and unexpected crashes during long-term operation.

* In the application code, this project uses static APIs, such as `xTaskCreateStatic`, `xQueueCreateStatic`, and `xSemaphoreCreateMutexStatic`, to allocate memory for kernel objects including tasks, queues, and mutexes. The repository's FreeRTOS configuration disables FreeRTOS dynamic allocation with `configSUPPORT_DYNAMIC_ALLOCATION 0`.
* Note: The upstream FreeRTOS example directories include demo dynamic-allocation examples, along with unmodified demo `FreeRTOSConfig` files. These examples do not affect this project's application-level static allocation policy. For engineering provability and safety, the project source code (`src/`) uses static allocation, and the build configuration removes `heap_4.c` to avoid introducing a runtime heap implementation.

### 3. Hardened Memory Boundary Management (MSP / PSP Isolation)

The project uses defensive design around the dual stack pointer mechanism of ARM Cortex-M, namely MSP and PSP:

* **System Startup Phase (MSP)**: The low-level stack size parameter was refactored with `-DSTACK_SIZE=2048`, fully resolving the startup main-stack overflow caused by deep standard C library `vsnprintf` stack usage in `main()`.
* **Task Runtime Phase (PSP)**: By scientifically quantifying the maximum stack cost of `vsnprintf`, each static task's local stack allocation is capped at `400` words (1600 bytes). This achieves an effective balance between safety and space efficiency under the strict 64 KB SRAM limit of the target chip.

### 4. Thread-Safe Real-Time Diagnostics Terminal (Interactive CLI Console)

The project designs and implements an asynchronous interactive command line similar to a Linux terminal:

* **Character Echo and Delete Support**: Supports typed-character echo and physical erasure through VT100 backspace escape handling.
* **Non-Blocking Command Parsing**: Through non-blocking UART reads combined with FreeRTOS queues, users can enter `status` to asynchronously query the running elevator state, such as position, direction, and motor state. This clearly demonstrates the concurrent multitasking advantages of a real-time operating system.

---

## 📁 Project Directory Structure

```text
/workspaces/mcu-elevator-qemu
├── CMakeLists.txt             # Top-level CMake project configuration file
├── cmake/
│   └── toolchain-arm-none-eabi.cmake  # ARM bare-metal cross-compilation toolchain configuration
├── standalone.ld              # Linker script for the QEMU-simulated chip (LM3S6965)
├── setup.sh                   # Dependency fetching and local environment check script (does not rewrite business source code)
├── src/
│   ├── main.c                 # System entry point: initializes hardware, static OS resources, and starts the scheduler
│   ├── FreeRTOSConfig.h       # FreeRTOS configuration: enables mutexes, static allocation, and assertion redirection
│   ├── bsp/                   # Low-level driver layer
│   │   ├── startup.c          # Chip startup file converted from assembly to C; defines the vector table and ResetISR
│   │   ├── bsp_uart.h         # UART abstraction header
│   │   └── bsp_uart.c         # UART register driver
│   ├── app/                   # Pure-C business logic layer
│   │   ├── elevator_fsm.h     # Elevator state machine header
│   │   └── elevator_fsm.c     # State transition logic
│   └── os_tasks/              # System tasks and mutex protection layer
│       ├── logger.h           # Thread-safe logger header
│       ├── logger.c           # Mutex-protected log output
│       ├── task_cli.h         # CLI interaction task header
│       └── task_cli.c         # Diagnostic console character-stream parser
```

---

## 🛠️ Prerequisites

This project runs in Ubuntu/Debian-based Linux containers, such as GitHub Codespaces. The required toolchain is listed below:

```bash
# Update package indexes and install the ARM cross-compiler, QEMU system emulator, and build tools
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi gdb-multiarch qemu-system-arm cmake make git
```

---

## 🚀 Dependency Setup

`setup.sh` is responsible only for local environment checks and external dependency fetching. It downloads or updates the official FreeRTOS repository and initializes the `FreeRTOS/Source` submodule. The script does not rewrite `src/`, `cmake/`, `CMakeLists.txt`, or other business source code, which prevents the initialization flow from overwriting local developer changes. The script also supports the `FREERTOS_REPO` and `FREERTOS_REF` environment variables so that CI systems or offline mirrors can pin the dependency source and version.

Run the following commands directly from the project root:

```bash
# 1. Grant execute permission to the script
chmod +x setup.sh

# 2. Fetch/update the FreeRTOS dependency
./setup.sh
```

### 📋 Automated Script Flow

1. **Tool Checks**: Requires `git`; emits missing-tool warnings for build/simulation tools such as `cmake`, `make`, `arm-none-eabi-gcc`, and `qemu-system-arm`, but does not block dependency fetching.
2. **Dependency Retrieval**: If `FreeRTOS/` does not exist, the script clones the official FreeRTOS repository and fetches its submodules.
3. **Safe Update**: If `FreeRTOS/` is already a git checkout, the script performs a fast-forward update and initializes submodules.
4. **Overwrite Protection**: If `FreeRTOS/` is a non-empty, non-git directory, the script asks whether to overwrite it. In non-interactive environments such as CI, it refuses to overwrite unless `./setup.sh --force` is explicitly passed.
5. **Interface Self-Check**: `./setup.sh --check-only` checks only script arguments and host-tool hints without downloading or modifying dependencies, which is suitable for validating the script entry point in CI.
6. **Dependency Validation**: Confirms that `FreeRTOS/FreeRTOS/Source/tasks.c` exists. Otherwise, it prints a clear error and exits.

---

## ⚙️ Build Guide

After dependency initialization, the project can be built with **CMake**. During the first configuration, you must explicitly pass the ARM bare-metal toolchain file. This ensures CMake selects `arm-none-eabi-gcc` before the `project()` language checks, instead of using the host compiler. During configuration, CMake checks for `FreeRTOS/FreeRTOS/Source/tasks.c`; if the dependency is missing, it clearly prompts you to run `./setup.sh` or pass an existing checkout through `-DFREERTOS_ROOT=/path/to/FreeRTOS`:

```bash
# 1. Configure the build directory with the ARM bare-metal toolchain file
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake

# 2. Build the firmware (produces the final RTOSDemo firmware image)
cmake --build build
```

---

## 🎮 QEMU Simulation and Human-Machine Interaction

From the `build` directory, run the following command to start the firmware on QEMU's Cortex-M3 virtual machine:

```bash
qemu-system-arm -M lm3s6965evb -kernel RTOSDemo -nographic
```

### 💻 Interactive Command Reference

After the system starts normally, the terminal prints:

```text
==============================================
  Elevator Diagnostics Console (CLI) Ready
  Type 'help' to see available commands.
==============================================
elevator> 
```

You can enter the following commands in the console for real-time device diagnostics and call interaction:

* **`help`**: Show the supported command menu.
* **`status`**: Query the elevator's physical state across tasks immediately.
* **`call <1-3>`**: Call the elevator. For example, enter `call 3` and press Enter. The elevator state machine wakes up and starts simulating physical movement. During this process, you can try entering `status` for concurrent querying:

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

### 🚪 How to Exit the Simulator

Because QEMU takes over the terminal, press the following key sequence to exit the simulator and return to the Linux console:

* Press **`Ctrl + A`** together, release them, and then press the **`X`** key.
