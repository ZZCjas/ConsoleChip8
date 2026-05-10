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
#include <conio.h>
#include <intrin.h>
#include <sstream>
#include <iomanip>
#include <set>
using namespace std;

void color(int x)
{
    if(x>=0 && x<=15)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),x);
    else
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),7);
}

struct EmuConfig
{
    int ops_per_frame=10;
    DWORD frame_ms=16;
    char pixel_char='#';
    bool sound_enabled=true;
};

EmuConfig loadConfig(const string& configPath="chip8.ini")
{
    EmuConfig cfg;
    ifstream file(configPath);
    if(!file.is_open())
    {
        ofstream newFile(configPath);
        if(newFile.is_open())
        {
            newFile<<"# CHIP8 Emulator Configuration File\n";
            newFile<<"ops_per_frame = 10\n";
            newFile<<"frame_ms = 16\n";
            newFile<<"pixel_char = #\n";
            newFile<<"sound_enabled = true\n";
            newFile.close();
            cout<<"Created default configuration file: "<<configPath<<endl;
            this_thread::sleep_for(chrono::seconds(1));
        }
        return cfg;
    }
    char bom[3];
    file.read(bom,3);
    if(!(bom[0]==(char)0xEF && bom[1]==(char)0xBB && bom[2]==(char)0xBF))
        file.seekg(0);
    string line;
    while(getline(file,line))
    {
        size_t start=line.find_first_not_of(" \t\r\n");
        if(start==string::npos) continue;
        if(line[start]=='#') continue;
        size_t eqPos=line.find('=');
        if(eqPos==string::npos) continue;
        string key=line.substr(start,eqPos-start);
        key.erase(key.find_last_not_of(" \t")+1);
        string value=line.substr(eqPos+1);
        start=value.find_first_not_of(" \t\r\n");
        if(start!=string::npos)
            value=value.substr(start);
        size_t commentPos=value.find('#');
        if(commentPos!=string::npos)
            value=value.substr(0,commentPos);
        value.erase(value.find_last_not_of(" \t\r\n")+1);
        if(key=="ops_per_frame")
            cfg.ops_per_frame=stoi(value);
        else if(key=="frame_ms")
            cfg.frame_ms=stoi(value);
        else if(key=="pixel_char" && !value.empty())
            cfg.pixel_char=value[0];
        else if(key=="sound_enabled")
            cfg.sound_enabled=(value=="true" || value=="1" || value=="yes");
    }
    return cfg;
}

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
    bool display[64*32];
    uint8_t keypad[16];
    char pixel_char;
    bool sound_enabled;
    atomic<bool> should_play_sound;
    atomic<bool> stop_sound_thread;
    thread sound_thread;
    LARGE_INTEGER timerFreq;
    LARGE_INTEGER lastTimerUpdate;
    static const double TIMER_PERIOD_MS;
public:
    set<uint16_t> breakpoints;
    uint8_t readMemory(uint16_t addr) const { return memory[addr]; }

    Chip8(char pixel, bool snd_enabled) : pixel_char(pixel), sound_enabled(snd_enabled), should_play_sound(false), stop_sound_thread(false)
    {
        memset(memory,0,sizeof(memory));
        memset(V,0,sizeof(V));
        I=0;
        pc=0x200;
        memset(stack,0,sizeof(stack));
        sp=0;
        delay_timer=0;
        sound_timer=0;
        memset(display,0,sizeof(display));
        memset(keypad,0,sizeof(keypad));
        const uint8_t fontset[80]=
        {
            0xF0,0x90,0x90,0x90,0xF0,0x20,0x60,0x20,0x20,0x70,
            0xF0,0x10,0xF0,0x80,0xF0,0xF0,0x10,0xF0,0x10,0xF0,
            0x90,0x90,0xF0,0x10,0x10,0xF0,0x80,0xF0,0x10,0xF0,
            0xF0,0x80,0xF0,0x90,0xF0,0xF0,0x10,0x20,0x40,0x40,
            0xF0,0x90,0xF0,0x90,0xF0,0xF0,0x90,0xF0,0x10,0xF0,
            0xF0,0x90,0xF0,0x90,0x90,0xE0,0x90,0xE0,0x90,0xE0,
            0xF0,0x80,0x80,0x80,0xF0,0xE0,0x90,0x90,0x90,0xE0,
            0xF0,0x80,0xF0,0x80,0xF0,0xF0,0x80,0xF0,0x80,0x80
        };
        memcpy(memory,fontset,80);
        srand((unsigned)time(nullptr));
        QueryPerformanceFrequency(&timerFreq);
        QueryPerformanceCounter(&lastTimerUpdate);
        sound_thread = thread([this]()
        {
            while(!stop_sound_thread)
            {
                if(should_play_sound && this->sound_enabled)
                    Beep(880,50);
                else
                    Sleep(50);
            }
        });
    }

    ~Chip8()
    {
        stop_sound_thread=true;
        if(sound_thread.joinable())
            sound_thread.join();
    }

    bool BootFromROM(const string& file)
    {
        breakpoints.clear();
        ifstream rom(file,ios::binary|ios::ate);
        if(!rom.is_open())
            return false;
        streamsize size=rom.tellg();
        if(size<0)
            return false;
        const size_t ROM_START=0x200;
        const size_t MAX_ROM_SIZE=4096-ROM_START;
        if(static_cast<size_t>(size)>MAX_ROM_SIZE)
            return false;
        rom.seekg(0,ios::beg);
        vector<char> buffer(size);
        if(!rom.read(buffer.data(),size))
            return false;
        rom.close();
        memcpy(memory+ROM_START,buffer.data(),static_cast<size_t>(size));
        pc=ROM_START;
        I=0;
        sp=0;
        memset(V,0,sizeof(V));
        memset(stack,0,sizeof(stack));
        delay_timer=0;
        sound_timer=0;
        memset(display,0,sizeof(display));
        QueryPerformanceCounter(&lastTimerUpdate);
        return true;
    }

    bool DumpState(const string& file)
    {
        ofstream dump(file,ios::binary);
        if(!dump.is_open())
            return false;
        dump.write(reinterpret_cast<const char*>(memory),sizeof(memory));
        dump.write(reinterpret_cast<const char*>(V),sizeof(V));
        dump.write(reinterpret_cast<const char*>(&I),sizeof(I));
        dump.write(reinterpret_cast<const char*>(&pc),sizeof(pc));
        dump.write(reinterpret_cast<const char*>(stack),sizeof(stack));
        dump.write(reinterpret_cast<const char*>(&sp),sizeof(sp));
        dump.write(reinterpret_cast<const char*>(&delay_timer),sizeof(delay_timer));
        dump.write(reinterpret_cast<const char*>(&sound_timer),sizeof(sound_timer));
        dump.write(reinterpret_cast<const char*>(display),sizeof(display));
        return dump.good();
    }

    bool LoadDump(const string& file)
    {
        breakpoints.clear();
        ifstream dump(file,ios::binary);
        if(!dump.is_open())
            return false;
        dump.read(reinterpret_cast<char*>(memory),sizeof(memory));
        dump.read(reinterpret_cast<char*>(V),sizeof(V));
        dump.read(reinterpret_cast<char*>(&I),sizeof(I));
        dump.read(reinterpret_cast<char*>(&pc),sizeof(pc));
        dump.read(reinterpret_cast<char*>(stack),sizeof(stack));
        dump.read(reinterpret_cast<char*>(&sp),sizeof(sp));
        dump.read(reinterpret_cast<char*>(&delay_timer),sizeof(delay_timer));
        dump.read(reinterpret_cast<char*>(&sound_timer),sizeof(sound_timer));
        dump.read(reinterpret_cast<char*>(display),sizeof(display));
        QueryPerformanceCounter(&lastTimerUpdate);
        return dump.good();
    }

    void emulateCycle()
    {
        uint16_t opcode=(memory[pc]<<8)|memory[pc+1];
        pc+=2;
        switch(opcode&0xF000)
        {
            case 0x0000:
                switch(opcode&0x00FF)
                {
                    case 0xE0: memset(display,0,sizeof(display)); break;
                    case 0xEE: if(sp>0) pc=stack[--sp]; break;
                }
                break;
            case 0x1000: pc=opcode&0x0FFF; break;
            case 0x2000: if(sp<16){ stack[sp++]=pc; pc=opcode&0x0FFF; } break;
            case 0x3000:
                {
                    uint8_t X=(opcode>>8)&0x0F;
                    if(V[X]==(opcode&0x00FF)) pc+=2;
                }
                break;
            case 0x4000:
                {
                    uint8_t X=(opcode>>8)&0x0F;
                    if(V[X]!=(opcode&0x00FF)) pc+=2;
                }
                break;
            case 0x5000:
                {
                    uint8_t X=(opcode>>8)&0x0F;
                    uint8_t Y=(opcode>>4)&0x0F;
                    if(V[X]==V[Y]) pc+=2;
                }
                break;
            case 0x6000: V[(opcode>>8)&0x0F]=opcode&0x00FF; break;
            case 0x7000: V[(opcode>>8)&0x0F]+=opcode&0x00FF; break;
            case 0x8000:
                switch(opcode&0x000F)
                {
                    case 0x0:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[X]=V[Y];
                        }
                        break;
                    case 0x1:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[X]|=V[Y];
                        }
                        break;
                    case 0x2:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[X]&=V[Y];
                        }
                        break;
                    case 0x3:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[X]^=V[Y];
                        }
                        break;
                    case 0x4:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            uint16_t sum=V[X]+V[Y];
                            V[0xF]=sum>0xFF;
                            V[X]=sum&0xFF;
                        }
                        break;
                    case 0x5:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[0xF]=V[X]>=V[Y];
                            V[X]-=V[Y];
                        }
                        break;
                    case 0x6:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            V[0xF]=V[X]&0x01;
                            V[X]>>=1;
                        }
                        break;
                    case 0x7:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t Y=(opcode>>4)&0x0F;
                            V[0xF]=V[Y]>=V[X];
                            V[X]=V[Y]-V[X];
                        }
                        break;
                    case 0xE:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            V[0xF]=(V[X]>>7)&0x01;
                            V[X]<<=1;
                        }
                        break;
                }
                break;
            case 0x9000:
                {
                    uint8_t X=(opcode>>8)&0x0F;
                    uint8_t Y=(opcode>>4)&0x0F;
                    if(V[X]!=V[Y]) pc+=2;
                }
                break;
            case 0xA000: I=opcode&0x0FFF; break;
            case 0xB000: pc=V[0]+(opcode&0x0FFF); break;
            case 0xC000:
                {
                    uint8_t X=(opcode>>8)&0x0F;
                    V[X]=(rand()%256)&(opcode&0x00FF);
                }
                break;
            case 0xD000:
                {
                    uint8_t X=V[(opcode>>8)&0x0F];
                    uint8_t Y=V[(opcode>>4)&0x0F];
                    uint8_t N=opcode&0x000F;
                    V[0xF]=0;
                    for(int row=0;row<N;row++)
                    {
                        uint8_t sprite=memory[I+row];
                        for(int col=0;col<8;col++)
                        {
                            if((sprite&(0x80>>col))==0) continue;
                            int x=(X+col)%64;
                            int y=(Y+row)%32;
                            int idx=y*64+x;
                            if(display[idx]) V[0xF]=1;
                            display[idx]^=1;
                        }
                    }
                }
                break;
            case 0xE000:
                switch(opcode&0x00FF)
                {
                    case 0x9E:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            if(keypad[V[X]]) pc+=2;
                        }
                        break;
                    case 0xA1:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            if(!keypad[V[X]]) pc+=2;
                        }
                        break;
                }
                break;
            case 0xF000:
                switch(opcode&0x00FF)
                {
                    case 0x07: V[(opcode>>8)&0x0F]=delay_timer; break;
                    case 0x15: delay_timer=V[(opcode>>8)&0x0F]; break;
                    case 0x18: sound_timer=V[(opcode>>8)&0x0F]; break;
                    case 0x1E: I+=V[(opcode>>8)&0x0F]; break;
                    case 0x0A:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            bool pressed=false;
                            for(int i=0;i<16;i++)
                                if(keypad[i]){ V[X]=i; pressed=true; break; }
                            if(!pressed) pc-=2;
                        }
                        break;
                    case 0x29: I=(V[(opcode>>8)&0x0F]&0x0F)*5; break;
                    case 0x33:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            uint8_t val=V[X];
                            memory[I]=val/100;
                            memory[I+1]=(val/10)%10;
                            memory[I+2]=val%10;
                        }
                        break;
                    case 0x55:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            for(int i=0;i<=X;i++) memory[I+i]=V[i];
                        }
                        break;
                    case 0x65:
                        {
                            uint8_t X=(opcode>>8)&0x0F;
                            for(int i=0;i<=X;i++) V[i]=memory[I+i];
                        }
                        break;
                }
                break;
        }
    }

    void render()
    {
        static HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
        COORD linePos={0,0};
        DWORD charsWritten;
        for(int row=0;row<32;++row)
        {
            char lineBuffer[129];
            linePos.Y=row+1;
            for(int col=0;col<64;++col)
            {
                bool pixel=display[row*64+col];
                lineBuffer[col*2]=pixel?pixel_char:' ';
                lineBuffer[col*2+1]=pixel?pixel_char:' ';
            }
            lineBuffer[128]='\0';
            WriteConsoleOutputCharacterA(hConsole,lineBuffer,128,linePos,&charsWritten);
        }
    }

    void updateKeyboard()
    {
        const int keyMapping[16]=
        {
            'X','1','2','3',
            'Q','W','E','A',
            'S','D','Z','C',
            '4','R','F','V'
        };
        for(int i=0;i<16;i++)
            keypad[i]=(GetAsyncKeyState(keyMapping[i])&0x8000)?1:0;
    }

    void updateTimers()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed_ms=(now.QuadPart-lastTimerUpdate.QuadPart)*1000.0/timerFreq.QuadPart;
        if(elapsed_ms>=TIMER_PERIOD_MS)
        {
            int ticks=static_cast<int>(elapsed_ms/TIMER_PERIOD_MS);
            if(ticks>0)
            {
                delay_timer=(delay_timer>ticks)?delay_timer-ticks:0;
                sound_timer=(sound_timer>ticks)?sound_timer-ticks:0;
                lastTimerUpdate.QuadPart+=static_cast<LONGLONG>(ticks*TIMER_PERIOD_MS*timerFreq.QuadPart/1000.0);
            }
        }
        should_play_sound=(sound_timer>0);
    }

    uint16_t getPC() const { return pc; }
    uint16_t getCurrentOpcode() const { return (memory[pc]<<8)|memory[pc+1]; }
    bool isBreakpointHit() const { return breakpoints.find(pc)!=breakpoints.end(); }
    string toHex(uint16_t val) const
    {
        stringstream ss;
        ss<<hex<<uppercase<<setw(4)<<setfill('0')<<val;
        return ss.str();
    }

    void toggleBreakpoint(uint16_t addr)
    {
        if(breakpoints.find(addr)!=breakpoints.end())
        {
            breakpoints.erase(addr);
            MessageBoxA(NULL,("Breakpoint removed at 0x"+toHex(addr)).c_str(),"Breakpoint",MB_OK);
        }
        else
        {
            breakpoints.insert(addr);
            MessageBoxA(NULL,("Breakpoint set at 0x"+toHex(addr)).c_str(),"Breakpoint",MB_OK);
        }
    }

    string getRegistersString() const
    {
        stringstream ss;
        ss<<"Registers V0-V15:\n";
        for(int i=0;i<16;++i)
        {
            ss<<"V"<<hex<<uppercase<<i<<": 0x"<<setw(2)<<setfill('0')<<(int)V[i];
            if((i+1)%4==0) ss<<"\n";
            else ss<<"   ";
        }
        return ss.str();
    }

    string getInternalStateString() const
    {
        stringstream ss;
        ss<<"I  : 0x"<<hex<<uppercase<<I<<"\n";
        ss<<"PC : 0x"<<pc<<"\n";
        ss<<"SP : 0x"<<uppercase<<(int)sp<<" (stack top index)\n";
        ss<<"Stack (16 words):\n";
        for(int i=0;i<16;++i)
        {
            ss<<"["<<dec<<i<<"] = 0x"<<hex<<setw(4)<<setfill('0')<<stack[i];
            if((i+1)%4==0) ss<<"\n";
            else ss<<"   ";
        }
        ss<<"Delay Timer: "<<dec<<(int)delay_timer<<"\n";
        ss<<"Sound Timer: "<<(int)sound_timer<<"\n";
        return ss.str();
    }

    string getAllStateString() const
    {
        return getRegistersString()+"\n\n"+getInternalStateString();
    }
};

const double Chip8::TIMER_PERIOD_MS=1000.0/60.0;

enum class AppState { MENU, RUNNING, PAUSED, DEBUG };

void showMemoryViewer(Chip8& chip)
{
    const int ADDR_START=0x0000;
    const int ADDR_END=0x1000;
    const int BYTES_PER_LINE=16;
    const int TOTAL_LINES=(ADDR_END-ADDR_START+BYTES_PER_LINE-1)/BYTES_PER_LINE;
    const int VIEW_HEIGHT=30;
    uint16_t selected_addr=chip.getPC();
    int scroll_line=(selected_addr/BYTES_PER_LINE)-VIEW_HEIGHT/2;
    if(scroll_line<0) scroll_line=0;
    if(scroll_line>TOTAL_LINES-VIEW_HEIGHT) scroll_line=TOTAL_LINES-VIEW_HEIGHT;
    if(scroll_line<0) scroll_line=0;
    bool exit_viewer=false;
    system("cls");
    while(!exit_viewer)
    {
        HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
        COORD pos={0,0};
        SetConsoleCursorPosition(hConsole,pos);
        cout<<"! : breakpoint    < : current PC    +/- : cursor\n";
        cout<<"PC = 0x"<<hex<<uppercase<<setw(4)<<setfill('0')<<chip.getPC()
            <<"   Selected: 0x"<<setw(4)<<setfill('0')<<selected_addr
            <<"   [UP/DOWN] Scroll & Move   [LEFT/RIGHT] Move   [Space] toggle breakpoint   [ESC/F5] Exit\n";
        for(int line=scroll_line;line<scroll_line+VIEW_HEIGHT && line<TOTAL_LINES;++line)
        {
            int base=line*BYTES_PER_LINE;
            color(7);
            cout<<"0x"<<hex<<uppercase<<setw(4)<<setfill('0')<<base<<": ";
            for(int col=0;col<BYTES_PER_LINE;++col)
            {
                int addr=base+col;
                if(addr>=ADDR_END) break;
                uint8_t byte=chip.readMemory(addr);
                bool is_pc=(addr==chip.getPC());
                bool is_bp=(chip.breakpoints.find(addr)!=chip.breakpoints.end());
                bool is_cursor=(addr==selected_addr);
                if(is_pc)
                    color(1);
                else if(is_bp)
                    color(4);
                else
                    color(7);
                cout<<setw(2)<<setfill('0')<<(int)byte<<" ";
                color(7);
                string bp_mark=is_bp?"!":" ";
                string pc_mark=is_pc?"<":" ";
                string cursor_mark=is_cursor?((is_bp?"-":"+")):" ";
                cout<<bp_mark<<pc_mark<<cursor_mark<<" ";
            }
            cout<<"\n";
        }
        int key=_getch();
        if(key==0xE0)
        {
            key=_getch();
            int delta=0;
            if(key==0x48) delta=-BYTES_PER_LINE;
            else if(key==0x50) delta=BYTES_PER_LINE;
            else if(key==0x4B) delta=-1;
            else if(key==0x4D) delta=1;
            if(delta!=0)
            {
                int new_addr=selected_addr+delta;
                if(new_addr>=ADDR_START && new_addr<ADDR_END)
                    selected_addr=new_addr;
                else if(delta==-1 && selected_addr>ADDR_START)
                    selected_addr--;
                else if(delta==1 && selected_addr<ADDR_END-1)
                    selected_addr++;
                else if(delta==-BYTES_PER_LINE && selected_addr>=BYTES_PER_LINE)
                    selected_addr-=BYTES_PER_LINE;
                else if(delta==BYTES_PER_LINE && selected_addr+BYTES_PER_LINE<ADDR_END)
                    selected_addr+=BYTES_PER_LINE;
                int selected_line=selected_addr/BYTES_PER_LINE;
                if(selected_line<scroll_line)
                    scroll_line=selected_line;
                else if(selected_line>=scroll_line+VIEW_HEIGHT)
                    scroll_line=selected_line-VIEW_HEIGHT+1;
                if(scroll_line<0) scroll_line=0;
                if(scroll_line>TOTAL_LINES-VIEW_HEIGHT) scroll_line=TOTAL_LINES-VIEW_HEIGHT;
                if(scroll_line<0) scroll_line=0;
            }
        }
        else if(key==' ')
            chip.toggleBreakpoint(selected_addr);
        else if(key==27 || key==VK_F5)
            exit_viewer=true;
    }
}

void ShowConsoleCursor(bool show)
{
    HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci;
    GetConsoleCursorInfo(hConsole,&cci);
    cci.bVisible=show;
    SetConsoleCursorInfo(hConsole,&cci);
}

void ClearConsoleInputBuffer()
{
    HANDLE hStdin=GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hStdin);
    cin.clear();
}

void DrawTopBar(AppState state, bool romLoaded)
{
    HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
    COORD topLeft={0,0};
    DWORD written;
    const char* bar=nullptr;
    if(state==AppState::MENU)
        bar="[F1] Load ROM   [F2] Load Dump   [F3] About   [F4] Exit";
    else
    {
        const char* pauseText=(state==AppState::RUNNING)?"Pause":"Resume";
        static string barStr;
        barStr="[F1] "+string(pauseText)+"   [F2] Dump   [F3] About   [F4] Back to Menu   [F5] Debug";
        bar=barStr.c_str();
    }
    char line[128];
    memset(line,' ',127);
    line[127]='\0';
    WriteConsoleOutputCharacterA(hConsole,line,127,topLeft,&written);
    WriteConsoleOutputCharacterA(hConsole,bar,strlen(bar),topLeft,&written);
}

void DrawDebugTopBar(Chip8& chip)
{
    HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
    COORD topLeft={0,0};
    DWORD written;
    uint16_t pc=chip.getPC();
    uint16_t opcode=chip.getCurrentOpcode();
    char buffer[256];
    snprintf(buffer,sizeof(buffer),
             "PC: 0x%04X  Opcode: 0x%04X   [F1] Step   [F2] Show Full State   [F3] Breakpoint   [F4] Continue   [F5] Memory Viewer",
             pc,opcode);
    char clearLine[128];
    memset(clearLine,' ',127);
    clearLine[127]='\0';
    WriteConsoleOutputCharacterA(hConsole,clearLine,127,topLeft,&written);
    WriteConsoleOutputCharacterA(hConsole,buffer,strlen(buffer),topLeft,&written);
}

void ShowAbout()
{
    system("cls");
    cout<<R"(
 ¨€¨€¨€¨€¨€¨€¨[ ¨€¨€¨€¨€¨€¨€¨[ ¨€¨€¨€¨[   ¨€¨€¨[¨€¨€¨€¨€¨€¨€¨€¨[ ¨€¨€¨€¨€¨€¨€¨[ ¨€¨€¨[     ¨€¨€¨€¨€¨€¨€¨€¨[ ¨€¨€¨€¨€¨€¨€¨[¨€¨€¨[  ¨€¨€¨[¨€¨€¨[¨€¨€¨€¨€¨€¨€¨[  ¨€¨€¨€¨€¨€¨[ 
¨€¨€¨X¨T¨T¨T¨T¨a¨€¨€¨X¨T¨T¨T¨€¨€¨[¨€¨€¨€¨€¨[  ¨€¨€¨U¨€¨€¨X¨T¨T¨T¨T¨a¨€¨€¨X¨T¨T¨T¨€¨€¨[¨€¨€¨U     ¨€¨€¨X¨T¨T¨T¨T¨a¨€¨€¨X¨T¨T¨T¨T¨a¨€¨€¨U  ¨€¨€¨U¨€¨€¨U¨€¨€¨X¨T¨T¨€¨€¨[¨€¨€¨X¨T¨T¨€¨€¨[
¨€¨€¨U     ¨€¨€¨U   ¨€¨€¨U¨€¨€¨X¨€¨€¨[ ¨€¨€¨U¨€¨€¨€¨€¨€¨€¨€¨[¨€¨€¨U   ¨€¨€¨U¨€¨€¨U     ¨€¨€¨€¨€¨€¨[  ¨€¨€¨U     ¨€¨€¨€¨€¨€¨€¨€¨U¨€¨€¨U¨€¨€¨€¨€¨€¨€¨X¨a¨^¨€¨€¨€¨€¨€¨X¨a
¨€¨€¨U     ¨€¨€¨U   ¨€¨€¨U¨€¨€¨U¨^¨€¨€¨[¨€¨€¨U¨^¨T¨T¨T¨T¨€¨€¨U¨€¨€¨U   ¨€¨€¨U¨€¨€¨U     ¨€¨€¨X¨T¨T¨a  ¨€¨€¨U     ¨€¨€¨X¨T¨T¨€¨€¨U¨€¨€¨U¨€¨€¨X¨T¨T¨T¨a ¨€¨€¨X¨T¨T¨€¨€¨[
¨^¨€¨€¨€¨€¨€¨€¨[¨^¨€¨€¨€¨€¨€¨€¨X¨a¨€¨€¨U ¨^¨€¨€¨€¨€¨U¨€¨€¨€¨€¨€¨€¨€¨U¨^¨€¨€¨€¨€¨€¨€¨X¨a¨€¨€¨€¨€¨€¨€¨€¨[¨€¨€¨€¨€¨€¨€¨€¨[¨^¨€¨€¨€¨€¨€¨€¨[¨€¨€¨U  ¨€¨€¨U¨€¨€¨U¨€¨€¨U     ¨^¨€¨€¨€¨€¨€¨X¨a
 ¨^¨T¨T¨T¨T¨T¨a ¨^¨T¨T¨T¨T¨T¨a ¨^¨T¨a  ¨^¨T¨T¨T¨a¨^¨T¨T¨T¨T¨T¨T¨a ¨^¨T¨T¨T¨T¨T¨a ¨^¨T¨T¨T¨T¨T¨T¨a¨^¨T¨T¨T¨T¨T¨T¨a ¨^¨T¨T¨T¨T¨T¨a¨^¨T¨a  ¨^¨T¨a¨^¨T¨a¨^¨T¨a      ¨^¨T¨T¨T¨T¨a 
)"<<endl;
    cout<<"Created by ZZCjas\n";
    cout<<"Github Repo Link: https://github.com/ZZCjas/ConsoleChip8\n";
    cout<<"\nKeyboard Mapping (CHIP-8 -> PC):\n";
    cout<<"  CHIP-8 Key   PC Key\n";
    cout<<"  -------------------\n";
    cout<<"      0          X\n";
    cout<<"      1          1\n";
    cout<<"      2          2\n";
    cout<<"      3          3\n";
    cout<<"      4          Q\n";
    cout<<"      5          W\n";
    cout<<"      6          E\n";
    cout<<"      7          A\n";
    cout<<"      8          S\n";
    cout<<"      9          D\n";
    cout<<"      A          Z\n";
    cout<<"      B          C\n";
    cout<<"      C          4\n";
    cout<<"      D          R\n";
    cout<<"      E          F\n";
    cout<<"      F          V\n"<<endl;
    cout<<"Press any key to continue...";
    ClearConsoleInputBuffer();
    _getch();
    system("cls");
}

class PreciseTimer
{
private:
    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
public:
    PreciseTimer()
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
    }
    void reset()
    {
        QueryPerformanceCounter(&start);
    }
    double elapsedSeconds() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart-start.QuadPart)/frequency.QuadPart;
    }
    double currentSeconds() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart)/frequency.QuadPart;
    }
    void sleepUntil(double targetSeconds)
    {
        while(elapsedSeconds()<targetSeconds) Sleep(1);
    }
};

// Helper functions to reduce redundancy
static string getUserInputPath(const string& prompt)
{
    ClearConsoleInputBuffer();
    ShowConsoleCursor(true);
    system("cls");
    cout<<prompt;
    string path;
    getline(cin,path);
    ShowConsoleCursor(false);
    return path;
}

static void showMessageAndWait(const string& msg)
{
    cout<<msg<<"Press any key to continue...";
    _getch();
}

int main()
{
    EmuConfig cfg=loadConfig();
    system("mode con cols=128 lines=34");
    HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci={1,FALSE};
    SetConsoleCursorInfo(hConsole,&cci);
    system("title ConsoleChip8");
    Chip8 chip(cfg.pixel_char,cfg.sound_enabled);
    AppState state=AppState::MENU;
    const int keyF1=VK_F1,keyF2=VK_F2,keyF3=VK_F3,keyF4=VK_F4,keyF5=VK_F5;
    bool lastF1=false,lastF2=false,lastF3=false,lastF4=false,lastF5=false;
    const double FRAME_SEC=cfg.frame_ms/1000.0;
    PreciseTimer frameTimer;
    bool romLoaded=false;
    system("cls");
    DrawTopBar(state,romLoaded);
    timeBeginPeriod(1);
    while(1)
    {
        bool curF1=(GetAsyncKeyState(keyF1)&0x8000)!=0;
        bool curF2=(GetAsyncKeyState(keyF2)&0x8000)!=0;
        bool curF3=(GetAsyncKeyState(keyF3)&0x8000)!=0;
        bool curF4=(GetAsyncKeyState(keyF4)&0x8000)!=0;
        bool curF5=(GetAsyncKeyState(keyF5)&0x8000)!=0;
        if(state==AppState::MENU)
        {
            if(curF1 && !lastF1)
            {
                string path=getUserInputPath("Enter ROM file path: ");
                if(chip.BootFromROM(path))
                {
                    romLoaded=true;
                    state=AppState::RUNNING;
                    system("cls");
                    DrawTopBar(state,romLoaded);
                    frameTimer.reset();
                    chip.render();
                }
                else
                {
                    showMessageAndWait("Failed to load ROM. ");
                    system("cls");
                    DrawTopBar(state,romLoaded);
                }
            }
            else if(curF2 && !lastF2)
            {
                string path=getUserInputPath("Enter dump file path: ");
                if(chip.LoadDump(path))
                {
                    romLoaded=true;
                    state=AppState::RUNNING;
                    system("cls");
                    DrawTopBar(state,romLoaded);
                    frameTimer.reset();
                    chip.render();
                }
                else
                {
                    showMessageAndWait("Failed to load dump. ");
                    system("cls");
                    DrawTopBar(state,romLoaded);
                }
            }
            else if(curF3 && !lastF3)
            {
                ClearConsoleInputBuffer();
                ShowAbout();
                DrawTopBar(state,romLoaded);
            }
            else if(curF4 && !lastF4)
            {
                ShowConsoleCursor(true);
                timeEndPeriod(1);
                return 0;
            }
            Sleep(50);
        }
        else if(state==AppState::RUNNING || state==AppState::PAUSED)
        {
            if(curF1 && !lastF1)
            {
                state=(state==AppState::RUNNING)?AppState::PAUSED:AppState::RUNNING;
                DrawTopBar(state,romLoaded);
                frameTimer.reset();
                if(state==AppState::RUNNING) chip.render();
            }
            else if(curF2 && !lastF2)
            {
                string path=getUserInputPath("Enter dump file path to save: ");
                if(chip.DumpState(path))
                    cout<<"Dump saved successfully.\n";
                else
                    cout<<"Failed to save dump.\n";
                showMessageAndWait("");
                system("cls");
                DrawTopBar(state,romLoaded);
                chip.render();
            }
            else if(curF3 && !lastF3)
            {
                ClearConsoleInputBuffer();
                ShowAbout();
                DrawTopBar(state,romLoaded);
                chip.render();
            }
            else if(curF4 && !lastF4)
            {
                state=AppState::MENU;
                chip.breakpoints.clear();
                romLoaded=false;
                system("cls");
                DrawTopBar(state,romLoaded);
                Sleep(50);
                lastF1=curF1; lastF2=curF2; lastF3=curF3; lastF4=curF4; lastF5=curF5;
                continue;
            }
            else if(curF5 && !lastF5 && (state==AppState::RUNNING || state==AppState::PAUSED))
            {
                state=AppState::DEBUG;
                system("cls");
                DrawDebugTopBar(chip);
                chip.render();
            }
            if(state==AppState::PAUSED)
            {
                Sleep(20);
                lastF1=curF1; lastF2=curF2; lastF3=curF3; lastF4=curF4; lastF5=curF5;
                continue;
            }
            if(state==AppState::RUNNING)
            {
                double frameStartAbs=frameTimer.currentSeconds();
                for(int i=0;i<cfg.ops_per_frame;++i)
                {
                    if(chip.isBreakpointHit())
                    {
                        state=AppState::DEBUG;
                        system("cls");
                        DrawDebugTopBar(chip);
                        chip.render();
                        string msg="Breakpoint hit at 0x"+chip.toHex(chip.getPC());
                        MessageBoxA(NULL,msg.c_str(),"Debugger",MB_OK|MB_ICONINFORMATION);
                        chip.render();
                        DrawDebugTopBar(chip);
                        break;
                    }
                    chip.emulateCycle();
                }
                chip.updateKeyboard();
                chip.updateTimers();
                chip.render();
                double targetEnd=frameStartAbs+FRAME_SEC;
                double remaining=targetEnd-frameTimer.currentSeconds();
                if(remaining>0.002)
                    Sleep(static_cast<DWORD>((remaining-0.001)*1000));
                while(frameTimer.currentSeconds()<targetEnd)
                    YieldProcessor();
            }
        }
        else if(state==AppState::DEBUG)
        {
            if(curF1 && !lastF1)
            {
                chip.emulateCycle();
                chip.updateKeyboard();
                chip.render();
                DrawDebugTopBar(chip);
                if(chip.isBreakpointHit())
                {
                    string msg="Breakpoint hit at 0x"+chip.toHex(chip.getPC());
                    MessageBoxA(NULL,msg.c_str(),"Debugger",MB_OK|MB_ICONINFORMATION);
                    chip.render();
                    DrawDebugTopBar(chip);
                }
            }
            else if(curF2 && !lastF2)
            {
                string msg=chip.getAllStateString();
                MessageBoxA(NULL,msg.c_str(),"CHIP-8 Full State",MB_OK|MB_ICONINFORMATION);
                chip.render();
                DrawDebugTopBar(chip);
            }
            else if(curF3 && !lastF3)
            {
                chip.toggleBreakpoint(chip.getPC());
                chip.render();
                DrawDebugTopBar(chip);
            }
            else if(curF4 && !lastF4)
            {
                uint16_t current_pc=chip.getPC();
                bool had_breakpoint=(chip.breakpoints.find(current_pc)!=chip.breakpoints.end());
                if(had_breakpoint)
                {
                    chip.breakpoints.erase(current_pc);
                    chip.emulateCycle();
                    chip.breakpoints.insert(current_pc);
                    chip.render();
                    DrawDebugTopBar(chip);
                }
                state=AppState::RUNNING;
                system("cls");
                DrawTopBar(state,romLoaded);
                frameTimer.reset();
                chip.render();
            }
            else if(curF5 && !lastF5)
            {
                showMemoryViewer(chip);
                system("cls");
                DrawDebugTopBar(chip);
                chip.render();
            }
            Sleep(20);
        }
        lastF1=curF1; lastF2=curF2; lastF3=curF3; lastF4=curF4; lastF5=curF5;
    }
    timeEndPeriod(1);
    return 0;
}
