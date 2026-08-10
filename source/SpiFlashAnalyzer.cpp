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

#include "SpiFlashAnalyzer.h"
#include "SpiFlashAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include "SpiFlashAnalyzerResults.h"
#include "SpiFlashConstants.h"
#include "SpiFlash.h"
#include <sstream>
#include <iomanip>

namespace
{
int GetFrameEndEdgeIndex(const U64* cachedClocks, int clockEdgeCount)
{
    return clockEdgeCount - 1 + ((cachedClocks[0] & 1) ? 0 : 1);
}
}

SpiFlashAnalyzer::SpiFlashAnalyzer()
    : Analyzer2()
    , mSettings(new SpiFlashAnalyzerSettings())
    , mSimulationInitialized(false)
    , mCachedClockCount(0)
    , mCachedClocks{}
    , mChipSelect(nullptr)
    , mClock(nullptr)
    , mClockIdleState(BIT_LOW)
    , mCommandEnd(0)
    , mCommandStart(0)
    , mContinuousReadCmd(nullptr)
    , mCurrentBusMode(UNDEFINED)
    , mD2(nullptr)
    , mD3(nullptr)
    , mDataLength(0)
    , mDefaultBusMode(UNDEFINED)
    , mDirIn(false)
    , mEndOfStopBitOffset(0)
    , mMiso(nullptr)
    , mMosi(nullptr)
    , mSampleRateHz(0)
    , mSerial(nullptr)
    , mStartOfStopBitOffset(0)
{
    mInOutOffset  = 0;
    mCommand      = "";
    mCommandName  = "";
    mCommandValid = false;
    mAddress      = "";
    mAddressValid = false;
    mMode         = "";
    mModeValid    = false;
    mDummy        = "";
    mDummyValid   = false;
    mData         = "";
    mDataValid    = false;
    SetAnalyzerSettings(mSettings.get());
}

SpiFlashAnalyzer::~SpiFlashAnalyzer()
{
    KillThread();
}

AnalyzerChannelData *SpiFlashAnalyzer::GetAnalyzerChannelData(Channel &channel)
{
    if (channel == UNDEFINED_CHANNEL)
    {
        return nullptr;
    }
    else
    {
        return Analyzer::GetAnalyzerChannelData(channel);
    }
}

void SpiFlashAnalyzer::SetupResults()
{
    mResults.reset(new SpiFlashAnalyzerResults(this, mSettings.get()));
    SetAnalyzerResults(mResults.get());

    // Enable FrameV2 output for Logic 2
#ifdef LOGIC2
    UseFrameV2();
#endif

    if (mSettings->mEnableCommandSummary)
    {
        if (mSettings->mChipSelect != UNDEFINED_CHANNEL)
        {
            mResults->AddChannelBubblesWillAppearOn(mSettings->mChipSelect);
        }
        else
        {
            mResults->AddChannelBubblesWillAppearOn(mSettings->mClock);
        }
    }

    if (mSettings->mMosi != UNDEFINED_CHANNEL)
    {
        mResults->AddChannelBubblesWillAppearOn(mSettings->mMosi);
    }

    if (mSettings->mMiso != UNDEFINED_CHANNEL)
    {
        mResults->AddChannelBubblesWillAppearOn(mSettings->mMiso);
    }
}

void SpiFlashAnalyzer::AddFrame(U64 start, U64 end, U64 d1, U64 d2, U8 type, U8 flags)
{
    Frame f;
    f.mStartingSampleInclusive = S64(start);
    f.mEndingSampleInclusive   = S64(end);
    f.mData1                   = d1;
    f.mData2                   = d2;
    f.mFlags                   = flags;
    f.mType                    = type;

    if (type != FT_CMD || mSettings->mEnableCommandSummary)
    {
        mResults->AddFrame(f);
        mResults->CommitResults();
    }

#ifdef LOGIC2
    // Add FrameV2 alongside legacy AddFrame
    // V2 Frames are mainly used in protocol decodes.
    const char *type_str = nullptr;
    const char *out_str  = "OUT";
    const char *in_str   = "IN";
    FrameV2 v2;
    auto to_hex = [](auto value, int width = 2) {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::setfill('0') << std::setw(width) << std::hex
            << static_cast<uint64_t>(value);
        return oss.str();
    };
    auto to_hex2 = [](auto value, int width = 2) {
        std::ostringstream oss;
        oss << std::uppercase << std::setfill('0') << std::setw(width) << std::hex << static_cast<uint64_t>(value);
        return oss.str();
    };
    switch (type)
    {
    case FT_CMD: {
        bool valid = false;
        if (mContinuousReadCmd != nullptr)
        {
            valid        = true;
            mCommandName = "QRIO";
            v2.AddString("Command", mCommand.c_str());
        }
        else if (mCommandValid)
        {
            valid         = true;
            mCommandValid = false;
            v2.AddString("Command", mCommand.c_str());
        }

        if (mAddressValid)
        {
            valid         = true;
            mAddressValid = false;
            v2.AddString("Address", mAddress.c_str());
        }

        if (mModeValid)
        {
            valid      = true;
            mModeValid = false;
            v2.AddString("Mode", mMode.c_str());
        }

        if (mDummyValid)
        {
            valid       = true;
            mDummyValid = false;
            v2.AddString("Dummy", mDummy.c_str());
        }

        if (mDataValid)
        {
            valid                  = true;
            mDataValid             = false;
            std::string dataLenStr = std::to_string(mDataLength);
            v2.AddString("Length", dataLenStr.c_str());
            v2.AddString("Data", mData.c_str());
        }

        if (valid)
        {
            // Filter RDSR and WREN based on settings
            if ((mCommandName == "RDSR" && !mSettings->mIncludeRDSR) ||
                (mCommandName == "WREN" && !mSettings->mIncludeWREN))
            {
                break;
            }
            mResults->AddFrameV2(v2, mCommandName.c_str(), start, end);
            mResults->CommitResults();
        }
        break;
    }
    case FT_CMD_BYTE: {
        std::string out_value         = to_hex(U8(d1));
        const SpiCmdData *commandData = spiFlash.GetCommand(mCurrentBusMode, U8(d1));
        mCommandName  = (commandData && !commandData->NamesEmpty()) ? commandData->mNames[0] : "Unknown";
        mCommand      = out_value;
        mCommandValid = true;
        mData         = "";
        mDataLength   = 0;
        break;
    }
    case FT_OUT_ADDR24: {
        std::string out_value = to_hex(U32(d1), 6);
        mAddress              = out_value;
        mAddressValid         = true;
        break;
    }
    case FT_OUT_ADDR32: {
        std::string out_value = to_hex(U32(d1), 8);
        mAddress              = out_value;
        mAddressValid         = true;
        break;
    }
    case FT_DUMMY: {
        std::string out_value = to_hex(U8(d1));
        mDummy                = out_value;
        mDummyValid           = true;
        break;
    }
    case FT_M: {
        std::string out_value = to_hex(U8(d1));
        mMode                 = out_value;
        mModeValid            = true;
        break;
    }
    case FT_OUT_BYTE: {
        std::string out_value = to_hex2(U8(d1));
        mData += " " + out_value;
        mDataValid = true;
        mDataLength++;
        break;
    }
    case FT_IN_BYTE: {
        std::string in_value = to_hex2(U8(d2));
        mData += " " + in_value;
        mDataValid = true;
        mDataLength++;
        break;
    }
    case FT_IN_OUT: {
        // TODO: reconsider how this is handled!
        std::string out_value = to_hex(U8(d1));
        std::string in_value  = to_hex(U8(d2));
        mData += (mData.empty() ? "" : " ") + out_value;
        mData += (mData.empty() ? "" : " ") + in_value;
        mDataValid = true;
        mDataLength += 2;
        break;
    }
    case FT_IN_REG: {
        std::string out_value = to_hex(U8(d2));
        mData += (mData.empty() ? "" : " ") + out_value;
        mDataLength++;
        mDataValid = true;
        break;
    }
    case FT_OUT_REG: {
        std::string in_value = to_hex(U8(d2));
        mData += (mData.empty() ? "" : " ") + in_value;
        mDataLength++;
        mDataValid = true;
        break;
    }
    default:
        type_str = "Error";
        mResults->AddFrameV2(v2, type_str, start, end);
        mResults->CommitResults();
        break;
    }
#endif
}

// TODO: Remove this once there is no going back in time
U64 pos;

void SpiFlashAnalyzer::Setup()
{
    mContinuousReadCmd = nullptr;
    mChipSelect        = GetAnalyzerChannelData(mSettings->mChipSelect);
    mClock             = GetAnalyzerChannelData(mSettings->mClock);
    mMosi              = GetAnalyzerChannelData(mSettings->mMosi);
    mMiso              = GetAnalyzerChannelData(mSettings->mMiso);
    mD2                = GetAnalyzerChannelData(mSettings->mD2);
    mD3                = GetAnalyzerChannelData(mSettings->mD3);
    mClockIdleState    = (mSettings->mSpiMode == SPI_MODE0) ? BIT_LOW : BIT_HIGH;
    mDefaultBusMode    = BusMode(mSettings->mBusMode);
    mCurrentBusMode    = mDefaultBusMode;
    // Continues read mode selected as starting point
    U8 manufacturer      = (U8)(mSettings->mContinuousRead >> 8);
    U8 code              = (U8)mSettings->mContinuousRead;
    const CmdSet *cmdSet = spiFlash.GetCommandSet(manufacturer);
    if (cmdSet)
    {
        const SpiCmdData *cmd = cmdSet->GetCommand(mCurrentBusMode, code);
        if (cmd != NULL)
        {
            mContinuousReadCmd = cmd;
            mCurrentBusMode    = BusMode(cmd->mModeData);
        }
    }
    mCachedClockCount = 0;
    pos               = 0;

    // Initialize FT_IN_OUT offset counter for upcoming commands
    mInOutOffset = 0;
}

void SpiFlashAnalyzer::AdvanceDataToAbsPosition(U64 AbsolutePosition)
{
    if (pos > AbsolutePosition)
    {
        return;
    }
    pos = AbsolutePosition;
    if (mMosi)
    {
        mMosi->AdvanceToAbsPosition(AbsolutePosition);
    }
    if (mMiso)
    {
        mMiso->AdvanceToAbsPosition(AbsolutePosition);
    }
    if (mD2)
    {
        mD2->AdvanceToAbsPosition(AbsolutePosition);
    }
    if (mD3)
    {
        mD3->AdvanceToAbsPosition(AbsolutePosition);
    }
}

void SpiFlashAnalyzer::CacheDropOlderClocks(U64 limit)
{
    int i;
    for (i = 0; i < mCachedClockCount; ++i)
    {
        if (mCachedClocks[i] >> 1 >= limit)
        {
            if (i > 0)
            {
                memmove(mCachedClocks, mCachedClocks + i, (mCachedClockCount - i) * sizeof(mCachedClocks[0]));
            }
            break;
        }
    }
    mCachedClockCount -= i;
}

void SpiFlashAnalyzer::CacheClock(int num, U64 lowerLimit)
{
    if (lowerLimit)
    {
        CacheDropOlderClocks(lowerLimit);
    }

    // No cached clocks, move clock forward
    if (mClock->GetSampleNumber() < lowerLimit)
    {
        mClock->AdvanceToAbsPosition(lowerLimit);
    }

    while (mCachedClockCount < num && mClock->DoMoreTransitionsExistInCurrentData())
    {
        mClock->AdvanceToNextEdge();
        if (mClock->GetBitState() == BIT_HIGH)
        {
            mCachedClocks[mCachedClockCount++] = (mClock->GetSampleNumber() << 1) + 1;
        }
        else
        {
            mCachedClocks[mCachedClockCount++] = mClock->GetSampleNumber() << 1;
        }
    }
}

void SpiFlashAnalyzer::AdvanceToCommandStart()
{
    // If CS is present just move to next falling edge
    if (mChipSelect != NULL)
    {
        if (mChipSelect->GetBitState() == BIT_HIGH)
        {
            mChipSelect->AdvanceToNextEdge();
        }
        else
        {
            mChipSelect->AdvanceToNextEdge();
            mChipSelect->AdvanceToNextEdge();
        }
        mCommandStart = mChipSelect->GetSampleNumber();

        CacheClock(8, mCommandStart);
        if (mCachedClockCount > 0)
        {
            bool clockHigh = mCachedClocks[0] & 1;
            // If mode is 0 or 3 and clock state is not matching mark error
            if ((mSettings->mSpiMode == 0 && !clockHigh) || (mSettings->mSpiMode == 3 && clockHigh))
            {
                mResults->AddMarker(mCommandStart, AnalyzerResults::ErrorSquare, mSettings->mClock);
            }
            else if (mSettings->mSpiMode == 0xFF)
            {
                // For auto mode take current clock state as idle state
                mClockIdleState = clockHigh ? BIT_HIGH : BIT_LOW;
            }
        }

        // Command ends at next rising edge of CS or at the end of data
        if (mChipSelect->DoMoreTransitionsExistInCurrentData())
        {
            mChipSelect->AdvanceToNextEdge();
            mCommandEnd = mChipSelect->GetSampleNumber();
        }
        else
        {
            mCommandEnd = ~0;
        }
    }
    else
    {
        // TODO: Rethink clocks !!!
        // Hardware generated clock should have some pattern
        U64 edges[SPI_FLASH_EDGES_BUF_SIZE];

        if (mSettings->mSpiMode == SPI_MODE0 && mClock->GetBitState() == BIT_HIGH)
        {
            mClock->AdvanceToNextEdge();
        }
        else if (mSettings->mSpiMode == SPI_MODE3 && mClock->GetBitState() == BIT_LOW)
        {
            mClock->AdvanceToNextEdge();
        }
        else
        {
            mClockIdleState = BIT_LOW;
        }

        U64 sample = mClock->GetSampleNumber();
        // Assume that clock is in idle now
        mClock->AdvanceToNextEdge();
        edges[0] = mClock->GetSampleNumber(); // rising edge
        mClock->AdvanceToNextEdge();
        edges[1] = mClock->GetSampleNumber(); // falling edge
        while (true)
        {
            mClock->AdvanceToNextEdge();
            edges[2] = mClock->GetSampleNumber(); // rising edge
            mClock->AdvanceToNextEdge();
            edges[3] = mClock->GetSampleNumber(); // falling edge
            int d1   = int(edges[1] - edges[0]);
            int d2   = int(edges[2] - edges[1]);
            int d3   = int(edges[3] - edges[2]);
            if (d3 == 0)
            {
                return;
            }
            // If positive pulses differ more than 10 % and 2 samples
            // or negative pulses differ from positive more than 30 % and 2 samples
            // let's move to place where clock is more stable
            if ((abs(d1 - d3) > 2 && (abs(d1 - d2) > d1 / 10)) || (abs(d2 - d1) > 2 && (abs(d1 - d2) > d1 / 30)))
            {
                edges[0] = edges[2];
                edges[1] = edges[3];
                continue;
            }
            mClock->AdvanceToAbsPosition(edges[0]);
            mCommandStart = mClock->GetSampleNumber();
            break;
        }
    }
    AdvanceDataToAbsPosition(mCommandStart);
}

U8 SpiFlashAnalyzer::GetBits(BusMode mode, bool dirIn)
{
    U8 b = 0;

    if (mode == SINGLE)
    {
        if (dirIn)
        {
            if (mMiso)
            {
                b = mMiso->GetBitState() == BIT_HIGH ? 1 : 0;
            }
        }
        else
        {
            if (mMosi)
            {
                b = mMosi->GetBitState() == BIT_HIGH ? 1 : 0;
            }
        }
    }
    else
    {
        if (mMosi)
        {
            b = mMosi->GetBitState() == BIT_HIGH ? 1 : 0;
        }
        if (mMiso)
        {
            b |= mMiso->GetBitState() == BIT_HIGH ? 2 : 0;
        }
    }
    if (mode == QUAD)
    {
        if (mD2)
        {
            b |= mD2->GetBitState() == BIT_HIGH ? 4 : 0;
        }
        if (mD3)
        {
            b |= mD3->GetBitState() == BIT_HIGH ? 8 : 0;
        }
    }

    return b;
}

void SpiFlashAnalyzer::AddSampleMarkers(U64 sample, BusMode mode, bool dirIn)
{
    if (!mSettings->mEnableSampleMarkers)
    {
        return;
    }

    if (mode == SINGLE)
    {
        if (dirIn)
        {
            if (mMiso)
            {
                mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMiso);
            }
        }
        else if (mMosi)
        {
            mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMosi);
        }

        return;
    }

    if (mMosi)
    {
        mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMosi);
    }
    if (mMiso)
    {
        mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMiso);
    }
    if (mode == QUAD)
    {
        if (mD2)
        {
            mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mD2);
        }
        if (mD3)
        {
            mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mD3);
        }
    }
}

void SpiFlashAnalyzer::AddMosiMisoSampleMarkers(U64 sample)
{
    if (mMosi)
    {
        mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMosi);
    }
    if (mMiso)
    {
        mResults->AddMarker(sample, AnalyzerResults::Dot, mSettings->mMiso);
    }
}

int SpiFlashAnalyzer::ExtractBits(U64 &start, U64 &end, U32 &val, U8 neededBits)
{
    BusMode mode = mCurrentBusMode;
    U8 bitCount  = 0;
    val          = 0;
    int i;
    int clockEdgeCount = 2 * neededBits / mode;

    CacheClock(clockEdgeCount + 1, mCommandStart);

    // Start time of first clock edge (rising or falling)
    start = mCachedClocks[0] >> 1;

    int frameEndEdgeIndex = GetFrameEndEdgeIndex(mCachedClocks, clockEdgeCount);

    // Not enough clocks to form a byte, and those clocks are in active CS?
    if (mCachedClockCount <= frameEndEdgeIndex || (mCachedClocks[frameEndEdgeIndex] >> 1) > mCommandEnd)
    {
        if (mCachedClockCount)
        {
            end = mCachedClocks[mCachedClockCount - 1] >> 1;
            if (end > mCommandEnd)
            {
                end = mCommandEnd;
            }
        }
        return -1;
    }

    // Let i point to rising edge time in table
    i = (mCachedClocks[0] & 1) ? 0 : 1;

    while (bitCount < neededBits)
    {
        AdvanceDataToAbsPosition(mCachedClocks[i] >> 1);
        mResults->AddMarker(mCachedClocks[i] >> 1, AnalyzerResults::UpArrow, mSettings->mClock);
        AddSampleMarkers(mCachedClocks[i] >> 1, mode, mDirIn);
        val <<= mode;
        val |= GetBits(mode, mDirIn);
        bitCount += mode;
        i += 2;
    }
    end = mCachedClocks[frameEndEdgeIndex] >> 1;
    CacheDropOlderClocks(end + 1);

    return 0;
}

int SpiFlashAnalyzer::ExtractMosiMiso(U64 &start, U64 &end, U8 &mosi, U8 &miso)
{
    int i;
    int ret           = 0;
    int clockEdgeCount = 8 * 2;
    mosi              = 0;
    miso              = 0;

    CacheClock(clockEdgeCount + 1, mCommandStart);

    // Start time of first clock edge (rising or falling)
    start = mCachedClocks[0] >> 1;

    int frameEndEdgeIndex = GetFrameEndEdgeIndex(mCachedClocks, clockEdgeCount);

    // Not enough clocks to form a byte, and those clocks are in active CS?
    if (mCachedClockCount <= frameEndEdgeIndex || (mCachedClocks[frameEndEdgeIndex] >> 1) > mCommandEnd)
    {
        end = mCachedClocks[mCachedClockCount - 1] >> 1;
        if (end > mCommandEnd)
        {
            end = mCommandEnd;
        }
        ret = -1;
    }
    else
    {
        // Let i point to rising edge time in table
        i = (mCachedClocks[0] & 1) ? 0 : 1;

        for (auto bitCount = 0; bitCount < 8; ++bitCount, i += 2)
        {
            AdvanceDataToAbsPosition(mCachedClocks[i] >> 1);
            //AddMosiMisoSampleMarkers(mCachedClocks[i] >> 1);
            if (mMosi)
            {
                mosi = (mosi << 1) + (mMosi->GetBitState() == BIT_HIGH ? 1 : 0);
            }
            if (mMiso)
            {
                miso = (miso << 1) + (mMiso->GetBitState() == BIT_HIGH ? 1 : 0);
            }
        }
        end = mCachedClocks[frameEndEdgeIndex] >> 1;
    }
    CacheDropOlderClocks(end + 1);

    return ret;
}

void SpiFlashAnalyzer::AnalyzeCommandBits()
{
    int b;
    U64 cmdExtra;
    U32 val;
    U32 addr = 0;
    U64 start;
    U64 end;
    U8 m;

    union
    {
        const SpiCmdData *data;
        intptr_t code;
    } cmd;

    cmd.data = nullptr;
    // Every packet starts with a new command.
    mResults->CommitPacketAndStartNewPacket();

    mDirIn = false;

    // Reset FT_IN_OUT per-command counter
    mInOutOffset = 0;

    do
    {
        cmdExtra = 0;
        if (mContinuousReadCmd != nullptr)
        {
            // TODO this seems sloppy. Maybe centralize this logic with AddFrame()
            cmd.data = mContinuousReadCmd;
            // Populate mCommand with the command name if available
            // if (!mContinuousReadCmd->mNames.empty())
            //	mCommand = mContinuousReadCmd->mNames[0];
            // else
            //	mCommand = "Unknown";
            // mCommandValid = true;
        }
        else
        {
            b = ExtractBits(start, end, val, 8);
            if (b < 0)
            {
                // Not enough bits for decoding command set value that is more then byte
                // but not enough for valid pointer
                cmd.code = SPI_FLASH_INVALID_CMD;
                break;
            }

            cmd.data = spiFlash.GetCommand(mCurrentBusMode, U8(val));
            if (cmd.data == nullptr)
            {
                cmd.code = (int)val;
            }

            // Add command to MOSI line
            AddFrame(start, end, val, reinterpret_cast<U64>(cmd.data), FT_CMD_BYTE, 0);
        }

        if (cmd.code > SPI_FLASH_INVALID_CMD)
        {
            UpdateBusMode((BusMode)cmd.data->mModeArgs);

            if (cmd.data->mAddressBits)
            {
                U32 addressLength = (cmd.data->mAddressBits != SPI_FLASH_ADDR_NOT_SET) ? cmd.data->mAddressBits :
                                                                                         mSettings->mAddressLength;

                addr = 0;

                if (ExtractBits(start, end, addr, addressLength) < 0)
                {
                    break;
                }

                // TODO: hard coded address field????
                AddFrame(start, end, addr, 0, FT_OUT_ADDR24, 0);
                cmdExtra = U64(addr) << 24;
            }

            if (cmd.data->mContinuousRead)
            {
                if (ExtractBits(start, end, val, 8) < 0)
                {
                    break;
                }

                m = U8(val);

                mContinuousReadCmd = ((m & 0x30) == 0x20) ? cmd.data : nullptr;
                AddFrame(start, end, val, 0, FT_M, 0);
            }

            U64 dummyStart = 0;
            U64 dummyEnd   = 0;

            if (cmd.data->mDummyBytes)
            {
                if (ExtractBits(dummyStart, dummyEnd, val, cmd.data->mDummyCount * 8) < 0)
                {
                    break;
                }
            }
            else if (cmd.data->mDummyCycles)
            {
                if (ExtractBits(dummyStart, dummyEnd, val, cmd.data->mDummyCycles) < 0)
                {
                    break;
                }
            }

            // Dummy cycles or byte found
            if (dummyEnd)
            {
                AddFrame(dummyStart, dummyEnd, val, 0, FT_DUMMY, 0);
                end = dummyEnd;
            }

            // Change bus mode if command require change for data phase
            UpdateBusMode(BusMode(cmd.data->mModeData));

            switch (cmd.data->mCmdOp)
            {
            case OP_DATA_WRITE:
                while (ExtractBits(start, end, val, 8) >= 0)
                {
                    // Pack the per-command byte offset into mData1
                    U64 packed = (U64(mInOutOffset) << 8) | U64(val);
                    AddFrame(start, end, packed, 0, FT_OUT_BYTE, 0);
                    cmdExtra++;
                    ++mInOutOffset;
                }
                break;
            case OP_DATA_READ:
                mDirIn = true;
                while (ExtractBits(start, end, val, 8) >= 0)
                {
                    // Pack the per-command byte offset into mData1
                    U64 packed = (U64(mInOutOffset) << 8) | U64(val);
                    AddFrame(start, end, 0, packed, FT_IN_BYTE, 0);
                    cmdExtra++;
                    ++mInOutOffset;
                }
                break;
            case OP_REG_WRITE:
                while (ExtractBits(start, end, val, 8) >= 0)
                {
                    AddFrame(start,
                             end,
                             val,
                             reinterpret_cast<U64>(cmd.data->GetRegister(size_t(cmdExtra))),
                             FT_OUT_REG,
                             0);
                    cmdExtra++;
                }
                break;
            case OP_REG_READ:
                mDirIn = true;
                while (ExtractBits(start, end, val, 8) >= 0)
                {
                    AddFrame(start,
                             end,
                             reinterpret_cast<U64>(cmd.data->GetRegister(size_t(cmdExtra))),
                             val,
                             FT_IN_REG,
                             0);
                    cmdExtra++;
                }
                break;
            }

            // Commands like Enter QPI or Exit QPI change bus mode
            if (cmd.data->mModeChange)
                mDefaultBusMode = BusMode(cmd.data->mModeChange);
        }
        else if (cmd.code < 0x100)
        {
            U8 miso, mosi;
            while (ExtractMosiMiso(start, end, mosi, miso) >= 0)
            {
                // Pack the per-command byte offset into mData1:
                // lower 8 bits = MOSI byte, upper bits = offset
                U64 packed = (U64(mInOutOffset) << 8) | U64(mosi);
                AddFrame(start, end, packed, miso, FT_IN_OUT, 0);
                ++mInOutOffset;
            }
        }
    } while (0);

    if (cmd.code != SPI_FLASH_INVALID_CMD)
    {
        AddFrame(mCommandStart, mCommandEnd, cmdExtra, reinterpret_cast<U64>(cmd.data), FT_CMD, 0);
        ReportProgress(mCommandEnd);
    }

    // Set default bus mode
    mCurrentBusMode = mDefaultBusMode;
}

void SpiFlashAnalyzer::WorkerThread()
{
    Setup();

    for (;;)
    {
        AdvanceToCommandStart();
        AnalyzeCommandBits();
        CheckIfThreadShouldExit();
    }
}

bool SpiFlashAnalyzer::NeedsRerun()
{
    return false;
}

U32 SpiFlashAnalyzer::GenerateSimulationData(U64 minimum_sample_index,
                                             U32 device_sample_rate,
                                             SimulationChannelDescriptor **simulation_channels)
{
    if (mSimulationInitialized == false)
    {
        mSimulationDataGenerator.Initialize(GetSimulationSampleRate(), mSettings.get());
        mSimulationInitialized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData(minimum_sample_index,
                                                           device_sample_rate,
                                                           simulation_channels);
}

U32 SpiFlashAnalyzer::GetMinimumSampleRateHz()
{
    return 100000; // TODO: Consider named constant if reused
}

const char *SpiFlashAnalyzer::GetAnalyzerName() const
{
    return "SPI Flash";
}

const char *GetAnalyzerName()
{
    return "SPI Flash";
}

Analyzer *CreateAnalyzer()
{
    return new SpiFlashAnalyzer();
}

void DestroyAnalyzer(Analyzer *analyzer)
{
    delete analyzer;
}
