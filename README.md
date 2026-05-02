```
 ██████╗ ██████╗ ███╗   ██╗███████╗ ██████╗ ██╗     ███████╗ ██████╗██╗  ██╗██╗██████╗  █████╗ 
██╔════╝██╔═══██╗████╗  ██║██╔════╝██╔═══██╗██║     ██╔════╝██╔════╝██║  ██║██║██╔══██╗██╔══██╗
██║     ██║   ██║██╔██╗ ██║███████╗██║   ██║██║     █████╗  ██║     ███████║██║██████╔╝╚█████╔╝
██║     ██║   ██║██║╚██╗██║╚════██║██║   ██║██║     ██╔══╝  ██║     ██╔══██║██║██╔═══╝ ██╔══██╗
╚██████╗╚██████╔╝██║ ╚████║███████║╚██████╔╝███████╗███████╗╚██████╗██║  ██║██║██║     ╚█████╔╝
 ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝ ╚═════╝ ╚══════╝╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝╚═╝      ╚════╝ 
```

This is a well‑optimized single file CHIP‑8 emulator that runs directly in the Windows console.

![LICENSE](https://camo.githubusercontent.com/382a3e1435f055f27a7b938f9152a25c8358abc88ed4312ade7cad5b237cb11d/68747470733a2f2f696d672e736869656c64732e696f2f6769746875622f6c6963656e73652f626974636f6f6b6965732f77696e7261722d6b657967656e2e7376673f6c6f676f3d676974687562)

## Features

- Full CHIP‑8 instruction set implementation  
- Configurable emulation speed (ops per frame, frame time)  
- Sound support with on/off toggle  
- Save / load emulator state (dump files)  
- On‑screen menu with function key controls  
- High performance and low RAM & CPU requirement
- Retro console UI

## System Requirements

- Any computer running Windows 2000 or later versions.
- At least 10KB of RAM
- A screen which can contain a normal sized console window.

## Building

### Requirements

- Windows OS  
- [MinGW‑w64](https://www.mingw-w64.org/) (or any C++11 compiler that supports Windows API)  

### Compile with MinGW

```bash
g++ -o ConsoleChip8.exe ConsoleChip8.cpp
```

### Compile with MSVC

```bash
cl ConsoleChip8.cpp /Fe:ConsoleChip8.exe
```

## Usage

1. Launch `ConsoleChip8.exe`  
2. Use the **menu bar** at the top of the console:

| Key | Menu mode                          | Run / Pause mode                    |
|-----|------------------------------------|-------------------------------------|
| F1  | Load ROM (.ch8)                    | Pause / Resume emulation             |
| F2  | Load dump state                    | Save current state as dump          |
| F3  | Show about / keyboard map          | Show about / keyboard map           |
| F4  | Exit                               | Return to main menu                 |

### Running a Game / ROM

- Press **F1** in the main menu, then enter the path to a CHIP‑8 ROM file (usually `.ch8`).  
- The emulator will start immediately.  
- Press **F1** again to pause/resume.

### Saving / Loading State

- While emulating, press **F2** to save the entire emulator state (CPU registers, memory, display) to a binary dump file.  
- From the main menu, press **F2** to load a previously saved dump and continue where you left off.

## Keyboard Mapping

CHIP‑8 uses a 16‑key hex keypad. The default mapping to a PC keyboard is:

| CHIP‑8 | PC Key |
|--------|--------|
| 0      | X      |
| 1      | 1      |
| 2      | 2      |
| 3      | 3      |
| 4      | Q      |
| 5      | W      |
| 6      | E      |
| 7      | A      |
| 8      | S      |
| 9      | D      |
| A      | Z      |
| B      | C      |
| C      | 4      |
| D      | R      |
| E      | F      |
| F      | V      |

## Configuration

On first run, the emulator creates a `chip8.cfg` file in the same directory. You can edit it to change:

```ini
# CHIP8 Emulator Configuration File
ops_per_frame = 10      # Number of CHIP‑8 instructions per frame
frame_ms = 16           # Frame duration in milliseconds (~60 FPS)
pixel_char = #          # Character used to draw filled pixels
sound_enabled = true    # Enable or disable beep sound
```

- `ops_per_frame` – Increase for faster execution, decrease for slower.  
- `frame_ms` – Controls the overall emulation speed.  
- `pixel_char` – Any printable character (e.g., `@`, `.`, `*`).  
- `sound_enabled` – Set to `false` to mute the internal Buzzer beep.

## Limitations

- Windows only. 
- No CHIP‑8‑X or Super‑CHIP extensions.  
- Sound uses the Buzzer (`Beep`) – may not work on all systems or may be limited.

## Troubleshooting

| Issue                        | Possible solution                                      |
|------------------------------|--------------------------------------------------------|
| Console flickers             | Adjust `frame_ms` (e.g., 16 → 20)                     |
| No sound                     | Check `sound_enabled = true` in config; ensure Buzzer is not disabled in Windows |
| ROM doesn't start            | Verify the ROM is a valid CHIP‑8 file (max 3584 bytes) |
| Keyboard unresponsive        | Make sure the console window is focused                |

---

Enjoy retrogaming on your terminal! 🕹️
