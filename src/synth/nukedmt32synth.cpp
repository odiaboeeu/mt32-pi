//
// nukedmt32synth.cpp
//

#include <circle/logger.h>

#include <cstring>

#include "config.h"
#include "mt32.h"
#include "ResamplerModel.h"
#include "reverb.h"
#include "synth/nukedmt32synth.h"

LOGMODULE("nukedmt32synth");

CNukedMT32Synth::CNukedMT32Synth(unsigned int nSampleRate)
    : CSynthBase(nSampleRate),
      m_pMT32(nullptr),
      m_pReverb(nullptr),
      m_pResamplerModel(nullptr),
      m_CurrentROMSet(TMT32ROMSet::Any),
      m_pControlROMImage(nullptr),
      m_pPCMROMImage(nullptr),
      m_nMasterVolume(100),
      m_bInitialized(false),
      m_LCDText{'\0'}
{
}

CNukedMT32Synth::~CNukedMT32Synth()
{
    ClearSynth();
}

void CNukedMT32Synth::ClearSynth()
{
    if (m_pResamplerModel)
    {
        SRCTools::ResamplerModel::freeResamplerModel(
            *m_pResamplerModel,
            *this
        );

        m_pResamplerModel = nullptr;
    }

    delete m_pReverb;
    m_pReverb = nullptr;

    delete m_pMT32;
    m_pMT32 = nullptr;

    m_bInitialized = false;
}

bool CNukedMT32Synth::Initialize()
{
    if (!m_ROMManager.ScanROMs())
    {
        LOGERR("No MT-32 ROM set available");
        return false;
    }

    TMT32ROMSet InitialROMSet = CConfig::Get()->MT32EmuROMSet;

    if (InitialROMSet != TMT32ROMSet::MT32Old &&
        InitialROMSet != TMT32ROMSet::MT32New)
    {
        InitialROMSet = TMT32ROMSet::Any;
    }

    if (!m_ROMManager.GetROMSet(
            InitialROMSet,
            m_CurrentROMSet,
            m_pControlROMImage,
            m_pPCMROMImage))
    {
        LOGERR("Failed to obtain MT-32 ROM set");
        return false;
    }

    if (m_CurrentROMSet != TMT32ROMSet::MT32Old &&
        m_CurrentROMSet != TMT32ROMSet::MT32New)
    {
        LOGERR("Nuked-MT32 currently supports MT-32 ROMs only");
        return false;
    }

    MT32Emu::File* const pControlFile =
        m_pControlROMImage->getFile();

    MT32Emu::File* const pPCMFile =
        m_pPCMROMImage->getFile();

    const size_t nExpectedControlSize =
        m_CurrentROMSet == TMT32ROMSet::MT32Old
            ? OldControlROMSize
            : NewControlROMSize;

    const size_t nControlSize = pControlFile->getSize();
    const size_t nPCMSize = pPCMFile->getSize();

    if (nControlSize != nExpectedControlSize)
    {
        LOGERR(
            "Unexpected Control ROM size: %u",
            static_cast<unsigned int>(nControlSize)
        );
        return false;
    }

    if (nPCMSize != PCMROMSize)
    {
        LOGERR(
            "Unexpected PCM ROM size: %u",
            static_cast<unsigned int>(nPCMSize)
        );
        return false;
    }

    const MT32Emu::Bit8u* const pControlData =
        pControlFile->getData();

    const MT32Emu::Bit8u* const pPCMData =
        pPCMFile->getData();

    if (!pControlData || !pPCMData)
    {
        LOGERR("MT-32 ROM data is unavailable");
        return false;
    }

    m_pMT32 = new mt32_t();

    if (!m_pMT32)
    {
        LOGERR("Failed to allocate Nuked-MT32 instance");
        return false;
    }

    std::memset(m_pMT32->rom, 0, sizeof(m_pMT32->rom));
    std::memcpy(
        m_pMT32->rom,
        pControlData,
        nControlSize
    );

    std::memcpy(
        m_pMT32->pcm,
        pPCMData,
        nPCMSize
    );

    m_pMT32->old_machine =
        m_CurrentROMSet == TMT32ROMSet::MT32Old;

    m_pReverb = new Mt32Reverb();

    if (!m_pReverb)
    {
        LOGERR("Failed to allocate Nuked-MT32 reverb");
        ClearSynth();
        return false;
    }

    m_pReverb->init();

    const char* const pModel =
        m_pMT32->old_machine ? "MT-32 old" : "MT-32 new";

    std::strncpy(
        m_LCDText,
        pModel,
        sizeof(m_LCDText) - 1
    );
    m_LCDText[sizeof(m_LCDText) - 1] = '\0';

    m_bInitialized = true;

    m_pResamplerModel =
        &SRCTools::ResamplerModel::createResamplerModel(
            *this,
            static_cast<double>(NativeSampleRate),
            static_cast<double>(m_nSampleRate),
            SRCTools::ResamplerModel::GOOD
        );

    LOGNOTE(
        "Nuked-MT32 audio path: %u Hz to %u Hz",
        NativeSampleRate,
        m_nSampleRate
    );

    LOGNOTE(
        "Nuked-MT32 initialized with %s ROM set",
        pModel
    );

    return true;
}

unsigned int CNukedMT32Synth::GetShortMessageLength(u8 nStatus)
{
    if (nStatus >= 0xF8)
        return 1;

    if (nStatus >= 0xF0)
    {
        switch (nStatus)
        {
            case 0xF1:
            case 0xF3:
                return 2;

            case 0xF2:
                return 3;

            default:
                return 1;
        }
    }

    const u8 nType = nStatus & 0xF0;

    if (nType == 0xC0 || nType == 0xD0)
        return 2;

    return 3;
}

void CNukedMT32Synth::PostMIDIByte(u8 nByte)
{
    if (!m_bInitialized)
        return;

    m_pMT32->post_midi(nByte);
    m_pReverb->observeMidiByte(nByte);
}

void CNukedMT32Synth::HandleMIDIShortMessage(u32 nMessage)
{
    if (!m_bInitialized)
        return;

    const u8 nStatus = static_cast<u8>(nMessage);
    const unsigned int nLength =
        GetShortMessageLength(nStatus);

    m_Lock.Acquire();

    for (unsigned int i = 0; i < nLength; ++i)
        PostMIDIByte(static_cast<u8>(nMessage >> (i * 8)));

    m_Lock.Release();

    CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CNukedMT32Synth::HandleMIDISysExMessage(
    const u8* pData,
    size_t nSize
)
{
    if (!m_bInitialized || !pData || nSize == 0)
        return;

    m_Lock.Acquire();

    for (size_t i = 0; i < nSize; ++i)
        PostMIDIByte(pData[i]);

    m_Lock.Release();
}

bool CNukedMT32Synth::IsActive()
{
    return m_bInitialized;
}

void CNukedMT32Synth::AllSoundOff()
{
    if (m_bInitialized)
    {
        m_Lock.Acquire();

        for (u8 nChannel = 0; nChannel < 16; ++nChannel)
        {
            PostMIDIByte(0xB0 | nChannel);
            PostMIDIByte(120);
            PostMIDIByte(0);
        }

        m_Lock.Release();
    }

    CSynthBase::AllSoundOff();
}

void CNukedMT32Synth::SetMasterVolume(u8 nVolume)
{
    if (nVolume > 100)
        nVolume = 100;

    m_Lock.Acquire();
    m_nMasterVolume = nVolume;
    m_Lock.Release();
}

void CNukedMT32Synth::getOutputSamples(
    float* pOutBuffer,
    unsigned int nFrames
)
{
    if (!pOutBuffer)
        return;

    if (!m_bInitialized)
    {
        std::memset(
            pOutBuffer,
            0,
            static_cast<size_t>(nFrames) * 2 * sizeof(*pOutBuffer)
        );

        return;
    }

    const float nGain =
        static_cast<float>(m_nMasterVolume) / 100.0f;

    unsigned int nRendered = 0;

    while (nRendered < nFrames)
    {
        unsigned int nChunk = nFrames - nRendered;

        if (nChunk > NativeBufferFrames)
            nChunk = NativeBufferFrames;

        m_pMT32->clock(nChunk);

        m_pReverb->process(
            &m_pMT32->samples[0][0],
            static_cast<int>(nChunk)
        );

        for (unsigned int i = 0; i < nChunk; ++i)
        {
            pOutBuffer[(nRendered + i) * 2] =
                (
                    static_cast<float>(
                        m_pMT32->samples[i][0]
                    ) / 32768.0f
                ) * nGain;

            pOutBuffer[(nRendered + i) * 2 + 1] =
                (
                    static_cast<float>(
                        m_pMT32->samples[i][1]
                    ) / 32768.0f
                ) * nGain;
        }

        nRendered += nChunk;
    }
}

size_t CNukedMT32Synth::Render(
    s16* pOutBuffer,
    size_t nFrames
)
{
    if (!pOutBuffer)
        return nFrames;

    m_Lock.Acquire();

    if (!m_bInitialized || !m_pResamplerModel)
    {
        std::memset(
            pOutBuffer,
            0,
            nFrames * 2 * sizeof(*pOutBuffer)
        );

        m_Lock.Release();
        return nFrames;
    }

    float ConversionBuffer[ConversionBufferFrames * 2];
    size_t nRendered = 0;

    while (nRendered < nFrames)
    {
        size_t nChunk = nFrames - nRendered;

        if (nChunk > ConversionBufferFrames)
            nChunk = ConversionBufferFrames;

        m_pResamplerModel->getOutputSamples(
            ConversionBuffer,
            static_cast<unsigned int>(nChunk)
        );

        for (size_t i = 0; i < nChunk * 2; ++i)
        {
            float nSample = ConversionBuffer[i];

            if (nSample > 1.0f)
                nSample = 1.0f;
            else if (nSample < -1.0f)
                nSample = -1.0f;

            const float nScaled =
                nSample >= 0.0f
                    ? nSample * 32767.0f
                    : nSample * 32768.0f;

            pOutBuffer[nRendered * 2 + i] =
                static_cast<s16>(nScaled);
        }

        nRendered += nChunk;
    }

    m_Lock.Release();

    return nFrames;
}

size_t CNukedMT32Synth::Render(
    float* pOutBuffer,
    size_t nFrames
)
{
    if (!pOutBuffer)
        return nFrames;

    m_Lock.Acquire();

    if (!m_bInitialized || !m_pResamplerModel)
    {
        std::memset(
            pOutBuffer,
            0,
            nFrames * 2 * sizeof(*pOutBuffer)
        );
    }
    else
    {
        m_pResamplerModel->getOutputSamples(
            pOutBuffer,
            static_cast<unsigned int>(nFrames)
        );
    }

    m_Lock.Release();

    return nFrames;
}

void CNukedMT32Synth::ReportStatus() const
{
    if (m_pUI)
        m_pUI->ShowSystemMessage("Nuked-MT32 ready");
}

void CNukedMT32Synth::UpdateLCD(
    CLCD& LCD,
    unsigned int nTicks
)
{
    (void)nTicks;

    if (m_bInitialized && m_pMT32->lcd_is_on())
    {
        const u8* const pText = m_pMT32->lcd_text();

        for (size_t i = 0; i < LCDTextLength; ++i)
        {
            const u8 nCharacter = pText[i];

            m_LCDText[i] =
                nCharacter >= 0x20 &&
                nCharacter < 0x7F
                    ? static_cast<char>(nCharacter)
                    : ' ';
        }

        m_LCDText[LCDTextLength] = '\0';
    }

    const u8 nStatusRow =
        LCD.GetType() == CLCD::TType::Character
            ? LCD.Height() - 1
            : LCD.Height() / 16 - 1;

    LCD.Print(
        m_LCDText,
        0,
        nStatusRow,
        true,
        false
    );
}
