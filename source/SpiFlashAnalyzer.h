/*
 * MIT License
 *
 * Copyright(c) 2026 Jon Carrier
 * Copyright(c) 2017 Jerzy Kasenberg (original author)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SPI_FLASH_ANALYZER_H
#define SPI_FLASH_ANALYZER_H

#include <Analyzer.h>
#include "SpiFlashAnalyzerResults.h"
#include "SpiFlashSimulationDataGenerator.h"
#include "SpiFlashConstants.h"
#include "SpiFlash.h"

class SpiFlashAnalyzerSettings;

class ANALYZER_EXPORT SpiFlashAnalyzer : public Analyzer2
{
public:
    SpiFlashAnalyzer();
    virtual ~SpiFlashAnalyzer();
    virtual void WorkerThread();

    virtual U32 GenerateSimulationData(U64 newest_sample_requested,
                                       U32 sample_rate,
                                       SimulationChannelDescriptor **simulation_channels);
    virtual U32 GetMinimumSampleRateHz();

    virtual const char *GetAnalyzerName() const;
    virtual bool NeedsRerun();

    AnalyzerChannelData *GetAnalyzerChannelData(Channel &channel);

protected: // vars
    std::unique_ptr<SpiFlashAnalyzerSettings> mSettings;
    std::unique_ptr<SpiFlashAnalyzerResults> mResults;
    AnalyzerChannelData *mSerial;

    SpiFlashSimulationDataGenerator mSimulationDataGenerator;
    bool mSimulationInitialized;

    AnalyzerChannelData *mChipSelect;
    AnalyzerChannelData *mClock;
    AnalyzerChannelData *mMosi;
    AnalyzerChannelData *mMiso;
    AnalyzerChannelData *mD2;
    AnalyzerChannelData *mD3;

    // Serial analysis vars:
    U32 mSampleRateHz;
    U32 mStartOfStopBitOffset;
    U32 mEndOfStopBitOffset;
    BusMode mCurrentBusMode;
    BusMode mDefaultBusMode;
    bool mDirIn;

    // Starting sample, CS activated
    U64 mCommandStart;
    // Ending sample, CS deactivated
    U64 mCommandEnd;
    BitState mClockIdleState;
    // Continues read mode active after CS is activated
    const SpiCmdData *mContinuousReadCmd;

private:
    void AddFrame(U64 start, U64 end, U64 d1, U64 d2, U8 type, U8 flags);
    void Setup();
    void AdvanceToCommandStart();
    void AdvanceDataToAbsPosition(U64 AbsolutePosition);
    void SetupResults();
    void AnalyzeCommandBits();

    void UpdateBusMode(BusMode mode)
    {
        if (mode)
        {
            mCurrentBusMode = mode;
        }
    }

    U8 GetBits(BusMode mode, bool dirIn);
    void AddSampleMarkers(U64 sample, BusMode mode, bool dirIn);
    void AddMosiMisoSampleMarkers(U64 sample);
    int ExtractBits(U64 &start, U64 &end, U32 &val, U8 bitCount);
    int ExtractMosiMiso(U64 &start, U64 &end, U8 &mosi, U8 &miso);

    void CacheClock(int num, U64 limit = 0);
    void CacheDropOlderClocks(U64 limit);

    U64 mCachedClocks[SPI_FLASH_CACHED_CLOCKS];
    U8 mCachedClockCount;

    // Tracks byte offset for FT_IN_OUT frames within the current command.
    // Each FT_IN_OUT added will use this offset (starting at zero) and then increment it.
    U32 mInOutOffset;

    // Used to hold onto related info to submit to FrameV2.
    // Results are submitted when the NCS line deasserts.
    // Validity is cleared when NCS asserts.
    // mData is resets to "" when instruction is received and accumulates each byte as a HEX OCTET.
    bool mCommandValid;
    std::string mCommand;
    std::string mCommandName;
    bool mAddressValid;
    std::string mAddress;
    bool mDummyValid;
    std::string mDummy;
    bool mModeValid;
    std::string mMode;
    bool mDataValid;
    std::string mData;
    U32 mDataLength;
};

extern "C" ANALYZER_EXPORT const char *__cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer *__cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer(Analyzer *analyzer);

#endif // SPI_FLASH_ANALYZER_H
