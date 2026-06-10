#include <stdint.h>

extern "C" {
int SC55_HeadlessLoadMk2RomSetFromMemory(
    const unsigned char* rom1_data, unsigned int rom1_size,
    const unsigned char* rom2_data, unsigned int rom2_size,
    const unsigned char* waverom1_data, unsigned int waverom1_size,
    const unsigned char* waverom2_data, unsigned int waverom2_size,
    const unsigned char* rom_sm_data, unsigned int rom_sm_size);

int SC55_HeadlessOpenAudio(int pageSize, int pageNum);
void SC55_HeadlessCloseAudio(void);
void SC55_HeadlessInit(void);
void SC55_HeadlessReset(void);
void SC55_HeadlessPostMIDIByte(unsigned char data);
void SC55_HeadlessRunStep(void);
int SC55_HeadlessPopSample(short* left, short* right);
}

extern "C" void SC55_LinkProbe(void)
{
    volatile auto p0 = (void*)SC55_HeadlessLoadMk2RomSetFromMemory;
    volatile auto p1 = (void*)SC55_HeadlessOpenAudio;
    volatile auto p2 = (void*)SC55_HeadlessCloseAudio;
    volatile auto p3 = (void*)SC55_HeadlessInit;
    volatile auto p4 = (void*)SC55_HeadlessReset;
    volatile auto p5 = (void*)SC55_HeadlessPostMIDIByte;
    volatile auto p6 = (void*)SC55_HeadlessRunStep;
    volatile auto p7 = (void*)SC55_HeadlessPopSample;

    (void)p0;
    (void)p1;
    (void)p2;
    (void)p3;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
}
