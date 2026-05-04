# ConsoleChip8

**A single-file, zero-dependency CHIP-8 emulator for Windows Console with built-in debugger**

[![GitHub Stars](https://img.shields.io/github/stars/ZZCjas/ConsoleChip8)](https://github.com/ZZCjas/ConsoleChip8/stargazers)
[![License](https://img.shields.io/github/license/ZZCjas/ConsoleChip8)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://github.com/ZZCjas/ConsoleChip8)
[![Language](https://img.shields.io/badge/Language-C++11-orange)](https://github.com/ZZCjas/ConsoleChip8)

```
 ██████╗ ██████╗ ███╗   ██╗███████╗ ██████╗ ██╗     ███████╗ ██████╗██╗  ██╗██╗██████╗  █████╗ 
██╔════╝██╔═══██╗████╗  ██║██╔════╝██╔═══██╗██║     ██╔════╝██╔════╝██║  ██║██║██╔══██╗██╔══██╗
██║     ██║   ██║██╔██╗ ██║███████╗██║   ██║██║     █████╗  ██║     ███████║██║██████╔╝╚█████╔╝
██║     ██║   ██║██║╚██╗██║╚════██║██║   ██║██║     ██╔══╝  ██║     ██╔══██║██║██╔═══╝ ██╔══██╗
╚██████╗╚██████╔╝██║ ╚████║███████║╚██████╔╝███████╗███████╗╚██████╗██║  ██║██║██║     ╚█████╔╝
 ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝ ╚═════╝ ╚══════╝╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝╚═╝      ╚════╝ 
```

## Overview
ConsoleChip8 is a lightweight, **pure C++ CHIP-8 emulator** that runs natively in the Windows Console.
It requires **no external libraries, no runtimes, and no complicated setup**—just download and run.

Designed for both retro gaming and **emulator development learning**, it includes a **full-featured built-in debugger** for studying CHIP-8 opcodes and system behavior.

---

## Features
### 🎮 Core Emulation
- Full implementation of the **original CHIP-8 instruction set**
- Single-file, <1MB executable, **zero dependencies**
- Native Windows console rendering with **almost no flickering**
- Configurable emulation speed & frame timing
- Buzzer sound support with toggle
- Save / load emulator state (dump files)
- Persistent configuration file
- Low CPU & RAM usage (≈1MB RAM required)
- ![Running TETRIS.ch8](https://cdn.luogu.com.cn/upload/image_hosting/mznez8bt.png)

### 🔬 Built-in Debugger
- One-key debug mode entry (F5)
- **Step-by-step instruction execution**
- View full register state (V0–V15, I, PC, SP, stack, timers)
- **Memory Viewer**
- **Breakpoint system** (toggle at current PC)
- Auto-pause when breakpoint hit
- Real-time PC & opcode display
- Seamless switch between run/debug modes
- ![Debugger Menu](https://cdn.luogu.com.cn/upload/image_hosting/48cwwoie.png)
- ![Memory Viewer](https://cdn.luogu.com.cn/upload/image_hosting/ngblvant.png)

### 🎯 Usability
- On-screen function-key menu
- Clear keyboard mapping
- Retro terminal-style UI

---

## Quick Start
### 1. Download & Run
Go to [Releases](https://github.com/ZZCjas/ConsoleChip8/releases) and download the prebuilt executable.
Double-click `ConsoleChip8.exe` to launch.

### 2. Load a ROM
- Press `F1` in the main menu
- Enter the path to your CHIP8 ROM file
- Emulation starts automatically

### 3. Debug Mode (For Learning)
While running, press `F5` to enter debug mode and inspect CHIP-8 behavior step by step.

---

## Keyboard Shortcuts
### Menu Mode
- `F1` – Load ROM
- `F2` – Load state dump
- `F3` – About & keymap
- `F4` – Exit

### Run / Pause Mode
- `F1` – Pause / Resume
- `F2` – Save state dump
- `F3` – About & keymap
- `F4` – Return to menu
- `F5` – Enter debug mode

### Debug Mode
- `F1` – Step one instruction
- `F2` – Show full emulator state
- `F3` – Toggle breakpoint at current PC
- `F4` – Exit debug mode
- `F5` – Show Memory Viewer

---

## Keypad Mapping (CHIP‑8 → PC)
| CHIP-8 | PC Key | CHIP-8 | PC Key |
|--------|--------|--------|--------|
| 0      | X      | 8      | S      |
| 1      | 1      | 9      | D      |
| 2      | 2      | A      | Z      |
| 3      | 3      | B      | C      |
| 4      | Q      | C      | 4      |
| 5      | W      | D      | R      |
| 6      | E      | E      | F      |
| 7      | A      | F      | V      |

---

## Build from Source
Requires: Windows + C++11 compiler (MinGW‑w64 or MSVC)

### MinGW
```bash
g++ ConsoleChip8.cpp -o ConsoleChip8.exe -std=c++11 -O2
```

### MSVC
```bash
cl ConsoleChip8.cpp /Fe:ConsoleChip8.exe
```

---

## Configuration
A `chip8.cfg` file is auto-generated on first run. You can customize:
- `ops_per_frame` – Instructions executed per frame
- `frame_ms` – Frame duration (controls speed)
- `pixel_char` – Character used for pixels
- `sound_enabled` – Toggle buzzer sound

---

## License
MIT License. Free for personal, educational, and commercial use.

---

## Support
If this project helps you learn CHIP-8 or emulator development, please **star ⭐** the repository to support it!

---
