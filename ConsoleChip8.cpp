#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include <string>
#include <sstream>

using namespace std;

// ---------- 全局配置 ----------
struct EmuConfig
{
    int ops_per_frame = 10;
    DWORD frame_ms = 16;
    char pixel_char = '#';
};

EmuConfig loadConfig(const string& configPath = "chip8.cfg")
{
    EmuConfig cfg;
    ifstream file(configPath);
    if (!file.is_open())
    {
        // 配置文件不存在，自动创建默认配置
        ofstream newFile(configPath);
        if (newFile.is_open())
        {
            newFile << "# CHIP8 Emulator Configuration File\n";
            newFile << "ops_per_frame = 10\n";
            newFile << "frame_ms = 16\n";
            newFile << "pixel_char = #\n";
            newFile.close();
            cout << "Created default configuration file: " << configPath << endl;
        }
        return cfg;  // 返回默认值
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
        if (key == "ops_per_frame")
            cfg.ops_per_frame = stoi(value);
        else if (key == "frame_ms")
            cfg.frame_ms = stoi(value);
        else if (key == "pixel_char" && !value.empty())
            cfg.pixel_char = value[0];
    }
    return cfg;
}

// ---------- Chip8 模拟器核心 ----------
class Chip8
{
private:
    uint8_t memory[4096];   // 4KB 内存
    uint8_t V[16];          // 寄存器 V0～VF
    uint16_t I;             // 索引寄存器
    uint16_t pc;            // 程序计数器（初始 0x200）
    uint16_t stack[16];     // 堆栈（深度 16）
    uint8_t sp;             // 堆栈指针
    uint8_t delay_timer;    // 延迟定时器
    uint8_t sound_timer;    // 声音定时器
    bool display[64 * 32];  // 屏幕像素 (64x32)
    uint8_t keypad[16];     // 键盘状态
    char pixel_char;        // 像素显示字符

public:
    Chip8(char pixel) : pixel_char(pixel)
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

        // 加载内置字体
        const uint8_t fontset[80] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };
        memcpy(memory, fontset, 80);
        srand((unsigned)time(nullptr));
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
        return true;
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
            case 0xE0:
                memset(display, 0, sizeof(display));
                break;
            case 0xEE:
                if (sp > 0)
                    pc = stack[--sp];
                break;
            }
            break;
        case 0x1000:
            pc = opcode & 0x0FFF;
            break;
        case 0x2000:
            if (sp < 16)
            {
                stack[sp++] = pc;
                pc = opcode & 0x0FFF;
            }
            break;
        case 0x3000:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                if (V[X] == (opcode & 0x00FF))
                    pc += 2;
            }
            break;
        case 0x4000:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                if (V[X] != (opcode & 0x00FF))
                    pc += 2;
            }
            break;
        case 0x5000:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                uint8_t Y = (opcode >> 4) & 0x0F;
                if (V[X] == V[Y])
                    pc += 2;
            }
            break;
        case 0x6000:
            V[(opcode >> 8) & 0x0F] = opcode & 0x00FF;
            break;
        case 0x7000:
            V[(opcode >> 8) & 0x0F] += opcode & 0x00FF;
            break;
        case 0x8000:
            switch (opcode & 0x000F)
            {
            case 0x0:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[X] = V[Y];
                }
                break;
            case 0x1:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[X] |= V[Y];
                    V[0xF] = 0;
                }
                break;
            case 0x2:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[X] &= V[Y];
                    V[0xF] = 0;
                }
                break;
            case 0x3:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[X] ^= V[Y];
                    V[0xF] = 0;
                }
                break;
            case 0x4:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    uint16_t sum = V[X] + V[Y];
                    V[0xF] = sum > 0xFF;
                    V[X] = sum & 0xFF;
                }
                break;
            case 0x5:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[0xF] = V[X] >= V[Y];
                    V[X] -= V[Y];
                }
                break;
            case 0x6:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    V[0xF] = V[X] & 0x01;
                    V[X] >>= 1;
                }
                break;
            case 0x7:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t Y = (opcode >> 4) & 0x0F;
                    V[0xF] = V[Y] >= V[X];
                    V[X] = V[Y] - V[X];
                }
                break;
            case 0xE:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    V[0xF] = (V[X] >> 7) & 0x01;
                    V[X] <<= 1;
                }
                break;
            }
            break;
        case 0x9000:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                uint8_t Y = (opcode >> 4) & 0x0F;
                if (V[X] != V[Y])
                    pc += 2;
            }
            break;
        case 0xA000:
            I = opcode & 0x0FFF;
            break;
        case 0xB000:
            pc = V[0] + (opcode & 0x0FFF);
            break;
        case 0xC000:
            {
                uint8_t X = (opcode >> 8) & 0x0F;
                V[X] = (rand() % 256) & (opcode & 0x00FF);
            }
            break;
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
                        if ((sprite & (0x80 >> col)) == 0)
                            continue;
                        int x = (X + col) % 64;
                        int y = (Y + row) % 32;
                        int idx = y * 64 + x;
                        if (display[idx])
                            V[0xF] = 1;
                        display[idx] ^= 1;
                    }
                }
            }
            break;
        case 0xE000:
            switch (opcode & 0x00FF)
            {
            case 0x9E:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    if (keypad[V[X]])
                        pc += 2;
                }
                break;
            case 0xA1:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    if (!keypad[V[X]])
                        pc += 2;
                }
                break;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF)
            {
            case 0x07:
                V[(opcode >> 8) & 0x0F] = delay_timer;
                break;
            case 0x15:
                delay_timer = V[(opcode >> 8) & 0x0F];
                break;
            case 0x18:
                sound_timer = V[(opcode >> 8) & 0x0F];
                break;
            case 0x1E:
                I += V[(opcode >> 8) & 0x0F];
                break;
            case 0x0A:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    bool pressed = false;
                    for (int i = 0; i < 16; i++)
                    {
                        if (keypad[i])
                        {
                            V[X] = i;
                            pressed = true;
                            break;
                        }
                    }
                    if (!pressed)
                        pc -= 2;
                }
                break;
            case 0x29:
                I = (V[(opcode >> 8) & 0x0F] & 0x0F) * 5;
                break;
            case 0x33:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    uint8_t val = V[X];
                    memory[I] = val / 100;
                    memory[I+1] = (val / 10) % 10;
                    memory[I+2] = val % 10;
                }
                break;
            case 0x55:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    for (int i = 0; i <= X; i++)
                        memory[I+i] = V[i];
                }
                break;
            case 0x65:
                {
                    uint8_t X = (opcode >> 8) & 0x0F;
                    for (int i = 0; i <= X; i++)
                        V[i] = memory[I+i];
                }
                break;
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
            char lineBuffer[129]; // 64列 * 2字符
            linePos.Y = row;

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

    void updateKeyboard()
    {
        const int keyMapping[16] = {
            'X', '1', '2', '3', // 0 1 2 3
            'Q', 'W', 'E', 'A', // 4 5 6 7
            'S', 'D', 'Z', 'C', // 8 9 A B
            '4', 'R', 'F', 'V'  // C D E F
        };
        for (int i = 0; i < 16; i++)
        {
            keypad[i] = (GetAsyncKeyState(keyMapping[i]) & 0x8000) ? 1 : 0;
        }
    }

    void updateTimers()
    {
        if (delay_timer > 0)
            delay_timer--;
        if (sound_timer > 0)
        {
            sound_timer--;
            if (sound_timer == 0)
                Beep(880, 50);
        }
    }
};
int main(int argc, char* argv[])
{
    // 加载配置（若文件不存在会自动创建）
    EmuConfig cfg = loadConfig();

    // 设置控制台大小
    system("mode con cols=128 lines=32");
    // 隐藏光标
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci = {1, FALSE};
    SetConsoleCursorInfo(hConsole, &cci);
    system("title ConsoleChip8");

    // 清屏
    system("cls");

    Chip8 chip(cfg.pixel_char);
    string romPath;

    // 处理 ROM 加载
    if (argc >= 2)
    {
        romPath = argv[1];
    }
    else
    {
        cout << "Please enter binary file path: ";
        getline(cin, romPath);
        system("cls");  // 清屏准备运行
    }

    if (!chip.BootFromROM(romPath))
    {
        cerr << "Failed to load file: " << romPath << endl;
        cerr << "Press any key to exit..." << endl;
        cin.get();
        return 1;
    }

    // 主循环参数
    const DWORD FRAME_MS = cfg.frame_ms;
    const int OPS_PER_FRAME = cfg.ops_per_frame;

    DWORD lastTimer = GetTickCount();
    DWORD lastFrame = GetTickCount();

    while (true)
    {
        DWORD now = GetTickCount();

        // 执行一帧的指令
        for (int i = 0; i < OPS_PER_FRAME; ++i)
        {
            chip.emulateCycle();
        }

        // 处理输入
        chip.updateKeyboard();

        // 更新定时器 (60Hz)
        if (now - lastTimer >= FRAME_MS)
        {
            chip.updateTimers();
            lastTimer = now;
        }

        // 渲染屏幕 (60Hz)
        if (now - lastFrame >= FRAME_MS)
        {
            chip.render();
            lastFrame = now;
        }

        // 休眠释放 CPU
        Sleep(1);
    }

    return 0;
}
