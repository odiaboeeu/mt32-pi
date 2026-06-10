//
// sc55synth.cpp
//
// Experimental Nuked-SC55 backend for mt32-pi
//

#include <fatfs/ff.h>

#include <circle/logger.h>

#include "lcd/lcd.h"
#include "lcd/ui.h"
#include "synth/sc55synth.h"
#include "utility.h"

LOGMODULE("sc55synth");

extern "C"
{
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

// Kept intentionally so the linker can be forced to retain the experimental core.
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

CSC55Synth::CSC55Synth(unsigned nSampleRate)
        : CSynthBase(nSampleRate),
          m_bInitialized(false),
          m_nVolume(100)
{
}

CSC55Synth::~CSC55Synth()
{
        if (m_bInitialized)
                SC55_HeadlessCloseAudio();
}

void CSC55Synth::FreeROMBuffer(u8*& pData)
{
        delete[] pData;
        pData = nullptr;
}

bool CSC55Synth::LoadROMFile(const char* pPath, u8*& pOutData, unsigned int& nOutSize)
{
        pOutData = nullptr;
        nOutSize = 0;

        FIL File;
        FRESULT Result = f_open(&File, pPath, FA_READ);

        if (Result != FR_OK)
        {
                LOGERR("Failed to open ROM: %s", pPath);
                return false;
        }

        FSIZE_t nSize = f_size(&File);
        if (nSize == 0)
        {
                LOGERR("Empty ROM: %s", pPath);
                f_close(&File);
                return false;
        }

        pOutData = new u8[nSize];
        if (!pOutData)
        {
            LOGERR("Failed to allocate ROM buffer: %s", pPath);
            f_close(&File);
            return false;
        }

        UINT nRead = 0;
        Result = f_read(&File, pOutData, nSize, &nRead);
        f_close(&File);

        if (Result != FR_OK || nRead != nSize)
        {
                LOGERR("Failed to read ROM: %s", pPath);
                FreeROMBuffer(pOutData);
                return false;
        }

        nOutSize = static_cast<unsigned int>(nSize);
        LOGNOTE("Loaded ROM %s (%u bytes)", pPath, nOutSize);
        return true;
}

bool CSC55Synth::Initialize()
{
        u8* pROM1 = nullptr;
        u8* pROM2 = nullptr;
        u8* pWaveROM1 = nullptr;
        u8* pWaveROM2 = nullptr;
        u8* pROMSM = nullptr;

        unsigned int nROM1Size = 0;
        unsigned int nROM2Size = 0;
        unsigned int nWaveROM1Size = 0;
        unsigned int nWaveROM2Size = 0;
        unsigned int nROMSMSize = 0;

        bool bOK =
                LoadROMFile("roms/sc55/rom1.bin", pROM1, nROM1Size) &&
                LoadROMFile("roms/sc55/rom2.bin", pROM2, nROM2Size) &&
                LoadROMFile("roms/sc55/waverom1.bin", pWaveROM1, nWaveROM1Size) &&
                LoadROMFile("roms/sc55/waverom2.bin", pWaveROM2, nWaveROM2Size) &&
                LoadROMFile("roms/sc55/rom_sm.bin", pROMSM, nROMSMSize);

        if (bOK)
        {
                bOK = SC55_HeadlessLoadMk2RomSetFromMemory(
                        pROM1, nROM1Size,
                        pROM2, nROM2Size,
                        pWaveROM1, nWaveROM1Size,
                        pWaveROM2, nWaveROM2Size,
                        pROMSM, nROMSMSize);

                if (!bOK)
                        LOGERR("SC55_HeadlessLoadMk2RomSetFromMemory failed");
        }

        FreeROMBuffer(pROM1);
        FreeROMBuffer(pROM2);
        FreeROMBuffer(pWaveROM1);
        FreeROMBuffer(pWaveROM2);
        FreeROMBuffer(pROMSM);

        if (!bOK)
                return false;

        if (!SC55_HeadlessOpenAudio(512, 64))
        {
                LOGERR("SC55_HeadlessOpenAudio failed");
                return false;
        }

        SC55_HeadlessInit();

        // Experimental warm-up: allow the emulated MCUs/PCM to advance a little
        // before the first audio callback. Keep this small to avoid delaying boot.
        for (unsigned i = 0; i < 200000; ++i)
                SC55_HeadlessRunStep();

        m_bInitialized = true;
        LOGNOTE("Experimental Nuked-SC55 initialized");
        return true;
}

void CSC55Synth::HandleMIDIShortMessage(u32 nMessage)
{
        if (!m_bInitialized)
                return;

        const u8 nStatus = nMessage & 0xFF;
        const u8 nData1 = (nMessage >> 8) & 0xFF;
        const u8 nData2 = (nMessage >> 16) & 0xFF;

        m_Lock.Acquire();

        SC55_HeadlessPostMIDIByte(nStatus);

        if ((nStatus & 0xF0) != 0xC0 && (nStatus & 0xF0) != 0xD0 && nStatus < 0xF8)
        {
                SC55_HeadlessPostMIDIByte(nData1);
                SC55_HeadlessPostMIDIByte(nData2);
        }
        else if (nStatus < 0xF8)
        {
                SC55_HeadlessPostMIDIByte(nData1);
        }

        m_Lock.Release();

        CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CSC55Synth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
        if (!m_bInitialized || !pData || nSize == 0)
                return;

        m_Lock.Acquire();

        for (size_t i = 0; i < nSize; ++i)
                SC55_HeadlessPostMIDIByte(pData[i]);

        m_Lock.Release();
}

void CSC55Synth::AllSoundOff()
{
        // CC 120 / All Sound Off on all channels.
        for (u8 ch = 0; ch < 16; ++ch)
        {
                SC55_HeadlessPostMIDIByte(0xB0 | ch);
                SC55_HeadlessPostMIDIByte(120);
                SC55_HeadlessPostMIDIByte(0);
        }

        CSynthBase::AllSoundOff();
}

void CSC55Synth::SetMasterVolume(u8 nVolume)
{
        m_nVolume = nVolume;
}

size_t CSC55Synth::Render(s16* pOutBuffer, size_t nFrames)
{
        if (!m_bInitialized)
        {
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(s16));
                return nFrames;
        }

        m_Lock.Acquire();

        for (size_t i = 0; i < nFrames; ++i)
        {
                short left = 0;
                short right = 0;

                // Crude first-pass scheduler. We will tune this later.
                unsigned guard = 0;
                while (!SC55_HeadlessPopSample(&left, &right) && guard < 512)
                {
                        SC55_HeadlessRunStep();
                        ++guard;
                }

                pOutBuffer[i * 2 + 0] = static_cast<s16>((left * m_nVolume) / 100);
                pOutBuffer[i * 2 + 1] = static_cast<s16>((right * m_nVolume) / 100);
        }

        m_Lock.Release();

        return nFrames;
}

size_t CSC55Synth::Render(float* pOutBuffer, size_t nFrames)
{
        if (!m_bInitialized)
        {
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(float));
                return nFrames;
        }

        m_Lock.Acquire();

        for (size_t i = 0; i < nFrames; ++i)
        {
                short left = 0;
                short right = 0;

                unsigned guard = 0;
                while (!SC55_HeadlessPopSample(&left, &right) && guard < 512)
                {
                        SC55_HeadlessRunStep();
                        ++guard;
                }

                pOutBuffer[i * 2 + 0] = (left / 32768.0f) * (m_nVolume / 100.0f);
                pOutBuffer[i * 2 + 1] = (right / 32768.0f) * (m_nVolume / 100.0f);
        }

        m_Lock.Release();

        return nFrames;
}

void CSC55Synth::ReportStatus() const
{
        if (m_pUI)
                m_pUI->ShowSystemMessage("Nuked-SC55");
}

void CSC55Synth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
        (void)nTicks;
        LCD.Print("Nuked-SC55", 0, 0, true, false);
}
