//
// soundfontsynth_stub.cpp
//
// Stub SoundFont backend for experimental SC-55-only builds.
// This keeps the mt32-pi control/menu code linkable without linking the real
// FluidSynth backend.
//

#include <cstring>

#include "lcd/lcd.h"
#include "synth/soundfontsynth.h"

CSoundFontSynth::CSoundFontSynth(unsigned nSampleRate)
        : CSynthBase(nSampleRate),
          m_pSettings(nullptr),
          m_pSynth(nullptr),
          m_nVolume(100),
          m_nInitialGain(0.0f),
          m_nPercussionMask(0),
          m_nCurrentSoundFontIndex(0)
{
}

CSoundFontSynth::~CSoundFontSynth()
{
}

void CSoundFontSynth::FluidSynthLogCallback(int nLevel, const char* pMessage, void* pUser)
{
        (void)nLevel;
        (void)pMessage;
        (void)pUser;
}

bool CSoundFontSynth::Initialize()
{
        return false;
}

void CSoundFontSynth::HandleMIDIShortMessage(u32 nMessage)
{
        (void)nMessage;
}

void CSoundFontSynth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
        (void)pData;
        (void)nSize;
}

bool CSoundFontSynth::IsActive()
{
        return false;
}

void CSoundFontSynth::AllSoundOff()
{
}

void CSoundFontSynth::SetMasterVolume(u8 nVolume)
{
        m_nVolume = nVolume;
}

size_t CSoundFontSynth::Render(float* pOutBuffer, size_t nFrames)
{
        if (pOutBuffer)
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(float));

        return nFrames;
}

size_t CSoundFontSynth::Render(s16* pOutBuffer, size_t nFrames)
{
        if (pOutBuffer)
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(s16));

        return nFrames;
}

void CSoundFontSynth::ReportStatus() const
{
}

void CSoundFontSynth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
        (void)LCD;
        (void)nTicks;
}

bool CSoundFontSynth::SwitchSoundFont(size_t nIndex)
{
        (void)nIndex;
        return false;
}

bool CSoundFontSynth::Reinitialize(const char* pSoundFontPath, const TFXProfile* pFXProfile)
{
        (void)pSoundFontPath;
        (void)pFXProfile;
        return false;
}

void CSoundFontSynth::ResetMIDIMonitor()
{
}

#ifndef NDEBUG
void CSoundFontSynth::DumpFXSettings() const
{
}
#endif

bool CSoundFontSynth::ParseGMSysEx(const u8* pData, size_t nSize)
{
        (void)pData;
        (void)nSize;
        return false;
}

bool CSoundFontSynth::ParseRolandSysEx(const u8* pData, size_t nSize)
{
        (void)pData;
        (void)nSize;
        return false;
}

bool CSoundFontSynth::ParseYamahaSysEx(const u8* pData, size_t nSize)
{
        (void)pData;
        (void)nSize;
        return false;
}
