#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <limits>
#include <intrin.h>

using namespace std;

// ---------- 全局配置 ----------
struct EmuConfig
{
    int ops_per_frame = 10;
    DWORD frame_ms = 16;
    char pixel_char = '#';
    bool sound_enabled = true; // 新增：声音开关
};

EmuConfig loadConfig(const string& configPath = "chip8.cfg")
{
    EmuConfig cfg;
    ifstream file(configPath);
    if (!file.is_open())
    {
        ofstream newFile(configPath);
        if (newFile.is_open())
        {
            newFile << "# CHIP8 Emulator Configuration File\n";
            newFile << "ops_per_frame = 10\n";
            newFile << "frame_ms = 16\n";
            newFile << "pixel_char = #\n";
            newFile << "sound_enabled = true\n"; // 新增：默认声音开启
            newFile.close();
            cout << "Created default configuration file: " << configPath << endl;
            // 新增：等待1秒后再显示主页面
            this_thread::sleep_for(chrono::seconds(1));
        }
        return cfg;
    }

    string line;
    while (getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        size_t eqPos = line.find('=');
        if (eqPos == string::npos)
            continue;
        string key = line.substr(0, eqPos);
        string value = line.substr(eqPos + 1);
        // 去除value前后的空格
        size_t start = value.find_first_not_of(" \t");
        size_t end = value.find_last_not_of(" \t");
        if (start != string::npos && end != string::npos)
            value = value.substr(start, end - start + 1);
        
        if (key == "ops_per_frame")
            cfg.ops_per_frame = stoi(value);
        else if (key == "frame_ms")
            cfg.frame_ms = stoi(value);
        else if (key == "pixel_char" && !value.empty())
            cfg.pixel_char = value[0];
        else if (key == "sound_enabled") // 新增：读取声音配置
            cfg.sound_enabled = (value == "true" || value == "1" || value == "yes");
    }
    return cfg;
}

// ---------- Chip8 模拟器核心 ----------
class Chip8
{
private:
    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t I;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool display[64 * 32];
    uint8_t keypad[16];
    char pixel_char;
    bool sound_enabled; // 新增：保存声音配置
    atomic<bool> should_play_sound; // 新增：声音播放标志
    thread sound_thread; // 新增：声音播放线程

public:
    Chip8(char pixel, bool snd_enabled) : pixel_char(pixel), sound_enabled(snd_enabled), should_play_sound(false)
    {
        memset(memory, 0, sizeof(memory));
        memset(V, 0, sizeof(V));
        I = 0;
        pc = 0x200;
        memset(stack, 0, sizeof(stack));
        sp = 0;
        delay_timer = 0;
        sound_timer = 0;
        memset(display, 0, sizeof(display));
        memset(keypad, 0, sizeof(keypad));

        const uint8_t fontset[80] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
            0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
            0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
            0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
            0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
            0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0,
            0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
            0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80
        };
        memcpy(memory, fontset, 80);
        srand((unsigned)time(nullptr));

        // 新增：启动声音播放线程
        sound_thread = thread([this]() {
            while (true) {
                if (should_play_sound && this->sound_enabled) {
                    Beep(880, 150); // 持续蜂鸣，通过循环实现持续发声
                } else {
                    Sleep(50);
                }
            }
        });
        sound_thread.detach();
    }

    bool BootFromROM(const string& file)
    {
        ifstream rom(file, ios::binary | ios::ate);
        if (!rom.is_open())
            return false;

        streamsize size = rom.tellg();
        if (size < 0)
            return false;

        const size_t ROM_START = 0x200;
        const size_t MAX_ROM_SIZE = 4096 - ROM_START;
        if (static_cast<size_t>(size) > MAX_ROM_SIZE)
            return false;

        rom.seekg(0, ios::beg);
        vector<char> buffer(size);
        if (!rom.read(buffer.data(), size))
            return false;
        rom.close();

        memcpy(memory + ROM_START, buffer.data(), static_cast<size_t>(size));
        pc = ROM_START;
        I = 0;
        sp = 0;
        memset(V, 0, sizeof(V));
        memset(stack, 0, sizeof(stack));
        delay_timer = 0;
        sound_timer = 0;
        memset(display, 0, sizeof(display));
        return true;
    }

    bool DumpState(const string& file)
    {
        ofstream dump(file, ios::binary);
        if (!dump.is_open())
            return false;

        dump.write(reinterpret_cast<const char*>(memory), sizeof(memory));
        dump.write(reinterpret_cast<const char*>(V), sizeof(V));
        dump.write(reinterpret_cast<const char*>(&I), sizeof(I));
        dump.write(reinterpret_cast<const char*>(&pc), sizeof(pc));
        dump.write(reinterpret_cast<const char*>(stack), sizeof(stack));
        dump.write(reinterpret_cast<const char*>(&sp), sizeof(sp));
        dump.write(reinterpret_cast<const char*>(&delay_timer), sizeof(delay_timer));
        dump.write(reinterpret_cast<const char*>(&sound_timer), sizeof(sound_timer));
        dump.write(reinterpret_cast<const char*>(display), sizeof(display));
        return dump.good();
    }

    bool LoadDump(const string& file)
    {
        ifstream dump(file, ios::binary);
        if (!dump.is_open())
            return false;

        dump.read(reinterpret_cast<char*>(memory), sizeof(memory));
        dump.read(reinterpret_cast<char*>(V), sizeof(V));
        dump.read(reinterpret_cast<char*>(&I), sizeof(I));
        dump.read(reinterpret_cast<char*>(&pc), sizeof(pc));
        dump.read(reinterpret_cast<char*>(stack), sizeof(stack));
        dump.read(reinterpret_cast<char*>(&sp), sizeof(sp));
        dump.read(reinterpret_cast<char*>(&delay_timer), sizeof(delay_timer));
        dump.read(reinterpret_cast<char*>(&sound_timer), sizeof(sound_timer));
        dump.read(reinterpret_cast<char*>(display), sizeof(display));
        return dump.good();
    }

    void emulateCycle()
    {
        uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
        pc += 2;

        switch (opcode & 0xF000)
        {
        case 0x0000:
            switch (opcode & 0x00FF)
            {
            case 0xE0: memset(display, 0, sizeof(display)); break;
            case 0xEE: if (sp > 0) pc = stack[--sp]; break;
            }
            break;
        case 0x1000: pc = opcode & 0x0FFF; break;
        case 0x2000: if (sp < 16) { stack[sp++] = pc; pc = opcode & 0x0FFF; } break;
        case 0x3000: { uint8_t X = (opcode >> 8) & 0x0F; if (V[X] == (opcode & 0x00FF)) pc += 2; } break;
        case 0x4000: { uint8_t X = (opcode >> 8) & 0x0F; if (V[X] != (opcode & 0x00FF)) pc += 2; } break;
        case 0x5000: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; if (V[X] == V[Y]) pc += 2; } break;
        case 0x6000: V[(opcode >> 8) & 0x0F] = opcode & 0x00FF; break;
        case 0x7000: V[(opcode >> 8) & 0x0F] += opcode & 0x00FF; break;
        case 0x8000:
            switch (opcode & 0x000F)
            {
            case 0x0: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[X] = V[Y]; } break;
            case 0x1: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[X] |= V[Y]; V[0xF] = 0; } break;
            case 0x2: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[X] &= V[Y]; V[0xF] = 0; } break;
            case 0x3: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[X] ^= V[Y]; V[0xF] = 0; } break;
            case 0x4: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; uint16_t sum = V[X] + V[Y]; V[0xF] = sum > 0xFF; V[X] = sum & 0xFF; } break;
            case 0x5: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[0xF] = V[X] >= V[Y]; V[X] -= V[Y]; } break;
            case 0x6: { uint8_t X = (opcode >> 8) & 0x0F; V[0xF] = V[X] & 0x01; V[X] >>= 1; } break;
            case 0x7: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; V[0xF] = V[Y] >= V[X]; V[X] = V[Y] - V[X]; } break;
            case 0xE: { uint8_t X = (opcode >> 8) & 0x0F; V[0xF] = (V[X] >> 7) & 0x01; V[X] <<= 1; } break;
            }
            break;
        case 0x9000: { uint8_t X = (opcode >> 8) & 0x0F; uint8_t Y = (opcode >> 4) & 0x0F; if (V[X] != V[Y]) pc += 2; } break;
        case 0xA000: I = opcode & 0x0FFF; break;
        case 0xB000: pc = V[0] + (opcode & 0x0FFF); break;
        case 0xC000: { uint8_t X = (opcode >> 8) & 0x0F; V[X] = (rand() % 256) & (opcode & 0x00FF); } break;
        case 0xD000:
        {
            uint8_t X = V[(opcode >> 8) & 0x0F];
            uint8_t Y = V[(opcode >> 4) & 0x0F];
            uint8_t N = opcode & 0x000F;
            V[0xF] = 0;
            for (int row = 0; row < N; row++)
            {
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; col++)
                {
                    if ((sprite & (0x80 >> col)) == 0) continue;
                    int x = (X + col) % 64;
                    int y = (Y + row) % 32;
                    int idx = y * 64 + x;
                    if (display[idx]) V[0xF] = 1;
                    display[idx] ^= 1;
                }
            }
        }
        break;
        case 0xE000:
            switch (opcode & 0x00FF)
            {
            case 0x9E: { uint8_t X = (opcode >> 8) & 0x0F; if (keypad[V[X]]) pc += 2; } break;
            case 0xA1: { uint8_t X = (opcode >> 8) & 0x0F; if (!keypad[V[X]]) pc += 2; } break;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF)
            {
            case 0x07: V[(opcode >> 8) & 0x0F] = delay_timer; break;
            case 0x15: delay_timer = V[(opcode >> 8) & 0x0F]; break;
            case 0x18: sound_timer = V[(opcode >> 8) & 0x0F]; break;
            case 0x1E: I += V[(opcode >> 8) & 0x0F]; break;
            case 0x0A:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                bool pressed = false;
                for (int i = 0; i < 16; i++)
                    if (keypad[i]) { V[X] = i; pressed = true; break; }
                if (!pressed) pc -= 2;
            }
            break;
            case 0x29: I = (V[(opcode >> 8) & 0x0F] & 0x0F) * 5; break;
            case 0x33:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                uint8_t val = V[X];
                memory[I] = val / 100;
                memory[I+1] = (val / 10) % 10;
                memory[I+2] = val % 10;
            }
            break;
            case 0x55: { uint8_t X = (opcode >> 8) & 0x0F; for (int i = 0; i <= X; i++) memory[I+i] = V[i]; } break;
            case 0x65: { uint8_t X = (opcode >> 8) & 0x0F; for (int i = 0; i <= X; i++) V[i] = memory[I+i]; } break;
            }
            break;
        }
    }

    void render()
    {
        static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD linePos = {0, 0};
        DWORD charsWritten;

        for (int row = 0; row < 32; ++row)
        {
            char lineBuffer[129];
            linePos.Y = row + 1;
            for (int col = 0; col < 64; ++col)
            {
                bool pixel = display[row * 64 + col];
                lineBuffer[col*2] = pixel ? pixel_char : ' ';
                lineBuffer[col*2+1] = pixel ? pixel_char : ' ';
            }
            lineBuffer[128] = '\0';
            WriteConsoleOutputCharacterA(hConsole, lineBuffer, 128, linePos, &charsWritten);
        }
    }

    // 更新键盘，忽略指定的键（用于功能键）
    void updateKeyboard(const bool* forbiddenKeys = nullptr)
    {
        const int keyMapping[16] = {
            'X', '1', '2', '3',
            'Q', 'W', 'E', 'A',
            'S', 'D', 'Z', 'C',
            '4', 'R', 'F', 'V'
        };
        for (int i = 0; i < 16; i++)
        {
            if (forbiddenKeys && forbiddenKeys[keyMapping[i] & 0xFF])
                continue;
            keypad[i] = (GetAsyncKeyState(keyMapping[i]) & 0x8000) ? 1 : 0;
        }
    }

    void updateTimers()
    {
        if (delay_timer > 0) delay_timer--;
        if (sound_timer > 0)
        {
            sound_timer--;
            should_play_sound = true; // 新增：声音定时器不为0时持续发声
        }
        else
        {
            should_play_sound = false; // 新增：声音定时器为0时停止发声
        }
    }
};

// ---------- UI 菜单栏 ----------
enum class AppState
{
    MENU,
    RUNNING,
    PAUSED
};

void ShowConsoleCursor(bool show)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci;
    GetConsoleCursorInfo(hConsole, &cci);
    cci.bVisible = show;
    SetConsoleCursorInfo(hConsole, &cci);
}

void ClearConsoleInputBuffer()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hStdin);
    cin.clear();
    //cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void DrawTopBar(AppState state, bool romLoaded)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD topLeft = {0, 0};
    DWORD written;

    const char* bar = nullptr;
    if (state == AppState::MENU)
    {
        bar = "[F1] Load ROM   [F2] Load Dump   [F3] About   [F4] Exit";
    }
    else
    {
        const char* pauseText = (state == AppState::RUNNING) ? "Pause" : "Resume";
        static string barStr;
        barStr = "[F1] " + string(pauseText) + "   [F2] Dump   [F3] About   [F4] Back to Menu"; // 修改：F4改为返回主页面
        bar = barStr.c_str();
    }

    char line[128];
    memset(line, ' ', 127);
    line[127] = '\0';
    WriteConsoleOutputCharacterA(hConsole, line, 127, topLeft, &written);
    WriteConsoleOutputCharacterA(hConsole, bar, strlen(bar), topLeft, &written);
}

void ShowAbout()
{
    system("cls");
    cout << R"(
 ██████╗ ██████╗ ███╗   ██╗███████╗ ██████╗ ██╗     ███████╗ ██████╗██╗  ██╗██╗██████╗  █████╗ 
██╔════╝██╔═══██╗████╗  ██║██╔════╝██╔═══██╗██║     ██╔════╝██╔════╝██║  ██║██║██╔══██╗██╔══██╗
██║     ██║   ██║██╔██╗ ██║███████╗██║   ██║██║     █████╗  ██║     ███████║██║██████╔╝╚█████╔╝
██║     ██║   ██║██║╚██╗██║╚════██║██║   ██║██║     ██╔══╝  ██║     ██╔══██║██║██╔═══╝ ██╔══██╗
╚██████╗╚██████╔╝██║ ╚████║███████║╚██████╔╝███████╗███████╗╚██████╗██║  ██║██║██║     ╚█████╔╝
 ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝ ╚═════╝ ╚══════╝╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝╚═╝      ╚════╝ 
)" << endl;
    cout << "Created by ZZCjas\n";
    cout << "Github Repo Link: https://github.com/ZZCjas/ConsoleChip8\n";
    cout << "\nKeyboard Mapping (CHIP-8 -> PC):\n";
    cout << "  CHIP-8 Key   PC Key\n";
    cout << "  -------------------\n";
    cout << "      0          X\n";
    cout << "      1          1\n";
    cout << "      2          2\n";
    cout << "      3          3\n";
    cout << "      4          Q\n";
    cout << "      5          W\n";
    cout << "      6          E\n";
    cout << "      7          A\n";
    cout << "      8          S\n";
    cout << "      9          D\n";
    cout << "      A          Z\n";
    cout << "      B          C\n";
    cout << "      C          4\n";
    cout << "      D          R\n";
    cout << "      E          F\n";
    cout << "      F          V\n";
    cout << "Press any key to continue...";
    ClearConsoleInputBuffer();
    cin.get();
    system("cls");
}

// ---------- 精准计时辅助函数 ----------
class PreciseTimer
{
private:
    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
public:
    PreciseTimer() { QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start); }
    void reset() { QueryPerformanceCounter(&start); }
    double elapsedSeconds() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - start.QuadPart) / frequency.QuadPart;
    }
    void sleepUntil(double targetSeconds)
    {
        while (elapsedSeconds() < targetSeconds) Sleep(1);
    }
};

// ---------- 主函数 ----------
int main()
{
	ios::sync_with_stdio(0);
    EmuConfig cfg = loadConfig();
    system("mode con cols=128 lines=34");
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci = {1, FALSE};
    SetConsoleCursorInfo(hConsole, &cci);
    system("title ConsoleChip8");

    Chip8 chip(cfg.pixel_char, cfg.sound_enabled); // 修改：传入声音配置
    AppState state = AppState::MENU;
    bool romLoaded = false;

    const int keyF1 = VK_F1, keyF2 = VK_F2, keyF3 = VK_F3, keyF4 = VK_F4;

    // 上升沿检测上次状态
    bool lastF1 = false, lastF2 = false, lastF3 = false, lastF4 = false;

    // 功能键屏蔽，传递给 chip.updateKeyboard 时忽略
    bool forbidden[256] = {false};
    forbidden[keyF1] = true;
    forbidden[keyF2] = true;
    forbidden[keyF3] = true;
    forbidden[keyF4] = true;

    const double FRAME_SEC = cfg.frame_ms / 1000.0;
    PreciseTimer frameTimer;
    DWORD lastTimerTick = GetTickCount();

    system("cls");
    DrawTopBar(state, romLoaded);

    while (true)
    {
        bool curF1 = (GetAsyncKeyState(keyF1) & 0x8000) != 0;
        bool curF2 = (GetAsyncKeyState(keyF2) & 0x8000) != 0;
        bool curF3 = (GetAsyncKeyState(keyF3) & 0x8000) != 0;
        bool curF4 = (GetAsyncKeyState(keyF4) & 0x8000) != 0;

        // ---------- 菜单状态 ----------
        if (state == AppState::MENU)
        {
            if (curF1 && !lastF1)
            {
                ClearConsoleInputBuffer();
                ShowConsoleCursor(true);
                system("cls");
                cout << "Enter ROM file path: ";
                string path;
                getline(cin, path);
                ShowConsoleCursor(false);
                if (chip.BootFromROM(path))
                {
                    romLoaded = true;
                    state = AppState::RUNNING;
                    system("cls");
                    DrawTopBar(state, romLoaded);
                    frameTimer.reset();
                    lastTimerTick = GetTickCount();
                    chip.render();
                }
                else
                {
                    cout << "Failed to load ROM. Press any key to continue...";
                    cin.get();
                    system("cls");
                    DrawTopBar(state, romLoaded);
                }
            }
            else if (curF2 && !lastF2)
            { 
                ClearConsoleInputBuffer();
                ShowConsoleCursor(true);
                system("cls");
                cout << "Enter dump file path: ";
                string path;
                getline(cin, path);
                ShowConsoleCursor(false);
                if (chip.LoadDump(path))
                {
                    romLoaded = true;
                    state = AppState::RUNNING;
                    system("cls");
                    DrawTopBar(state, romLoaded);
                    frameTimer.reset();
                    lastTimerTick = GetTickCount();
                    chip.render();
                }
                else
                {
                    cout << "Failed to load dump. Press any key to continue...";
                    cin.get();
                    system("cls");
                    DrawTopBar(state, romLoaded);
                }
            }
            else if (curF3 && !lastF3)
            {
            	ClearConsoleInputBuffer();
                ShowAbout();
                DrawTopBar(state, romLoaded);
            }
            else if (curF4 && !lastF4)
            {
            	ShowConsoleCursor(true);
                return 0;
            }
            Sleep(50);
            lastF1 = curF1; lastF2 = curF2; lastF3 = curF3; lastF4 = curF4;
            continue;
        }

        // ---------- 运行或暂停状态 (romLoaded == true) ----------
        if (state == AppState::RUNNING || state == AppState::PAUSED)
        {
            // 功能键处理（上升沿）
            if (curF1 && !lastF1)
            {
                if (state == AppState::RUNNING)
                    state = AppState::PAUSED;
                else
                    state = AppState::RUNNING;
                DrawTopBar(state, romLoaded);
                frameTimer.reset();
                lastTimerTick = GetTickCount();
                if (state == AppState::RUNNING)
                    chip.render();   // 恢复时刷新画面
            }
            else if (curF2 && !lastF2)
            {
                ClearConsoleInputBuffer();
                ShowConsoleCursor(true);
                system("cls");
                cout << "Enter dump file path to save: ";
                string path;
                getline(cin, path);
                ShowConsoleCursor(false);
                if (chip.DumpState(path))
                    cout << "Dump saved successfully.\n";
                else
                    cout << "Failed to save dump.\n";
                cout << "Press any key to continue...";
                cin.get();
                system("cls");
                DrawTopBar(state, romLoaded);
                chip.render();
            }
            else if (curF3 && !lastF3)
            {
            	ClearConsoleInputBuffer();
                ShowAbout();
                DrawTopBar(state, romLoaded);
                chip.render();
            }
            else if (curF4 && !lastF4)
            {
                state = AppState::MENU;
                romLoaded = false;
                system("cls");
                DrawTopBar(state, romLoaded);
                Sleep(50);
            	lastF1 = curF1; lastF2 = curF2; lastF3 = curF3; lastF4 = curF4;
                continue;
            }

            // 暂停状态：只刷新 UI 不模拟
            if (state == AppState::PAUSED)
            {
                Sleep(20);
                lastF1 = curF1; lastF2 = curF2; lastF3 = curF3; lastF4 = curF4;
                continue;
            }

            // 模拟一帧
            double frameStart = frameTimer.elapsedSeconds();
            for (int i = 0; i < cfg.ops_per_frame; ++i)
                chip.emulateCycle();

            chip.updateKeyboard(forbidden);  // 忽略功能键

            DWORD now = GetTickCount();
            if (now - lastTimerTick >= cfg.frame_ms)
            {
                chip.updateTimers();
                lastTimerTick = now;
            }

            chip.render();

            double elapsed = frameTimer.elapsedSeconds() - frameStart;
            double remaining = FRAME_SEC - elapsed;
            if (remaining > 0.001)
            {
                Sleep(static_cast<DWORD>(remaining * 1000 * 0.9));
                while (frameTimer.elapsedSeconds() - frameStart < FRAME_SEC)
                    YieldProcessor();
            }
        }

        lastF1 = curF1; lastF2 = curF2; lastF3 = curF3; lastF4 = curF4;
    }

    return 0;
}
