//
// sc55synth.h
//
// Experimental Nuked-SC55 backend for mt32-pi
//

#ifndef _sc55synth_h
#define _sc55synth_h

#include <circle/types.h>

#include "synth/synthbase.h"

class CSC55Synth : public CSynthBase
{
public:
        explicit CSC55Synth(unsigned nSampleRate);
        virtual ~CSC55Synth() override;

        // CSynthBase
        virtual bool Initialize() override;
        virtual void HandleMIDIShortMessage(u32 nMessage) override;
        virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) override;
        virtual bool IsActive() override { return m_bInitialized; }
        virtual void AllSoundOff() override;
        virtual void SetMasterVolume(u8 nVolume) override;
        virtual size_t Render(s16* pOutBuffer, size_t nFrames) override;
        virtual size_t Render(float* pOutBuffer, size_t nFrames) override;
        virtual void ReportStatus() const override;
        virtual void UpdateLCD(CLCD& LCD, unsigned int nTicks) override;

private:
        bool LoadROMFile(const char* pPath, u8*& pOutData, unsigned int& nOutSize);
        void FreeROMBuffer(u8*& pData);

        bool m_bInitialized;
        u8 m_nVolume;
};

#endif
