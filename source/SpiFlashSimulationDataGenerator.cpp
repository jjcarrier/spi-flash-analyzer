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

#include <cstdlib>

#include "SpiFlashSimulationDataGenerator.h"
#include "SpiFlashAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

#include "SpiFlash.h"

namespace
{
BusMode NormalizeBusMode(int mode)
{
    switch (mode)
    {
    case SINGLE:
        return SINGLE;
    case DUAL:
        return DUAL;
    case QUAD:
        return QUAD;
    default:
        return SINGLE;
    }
}

enum SpiFlashSimulationStates
{
    CLOCK_LOW        = 0,
    MOSI_LOW         = 0,
    MISO_LOW         = 0,
    D2_LOW           = 0,
    D3_LOW           = 0,
    CLOCK_HIGH       = 2,
    MOSI_HIGH        = 4,
    MISO_HIGH        = 8,
    D2_HIGH          = 16,
    D3_HIGH          = 32,
    HALF_CLOCK_DELAY = 128
};

enum SpiChipSelectStates
{
    CS_LOW  = 0,
    CS_HIGH = 1
};
} // namespace

void SpiFlashSimulationDataGenerator::SetSpiMode(SpiMode mode)
{
    mSpiMode = mode;
}

uint8_t SpiFlashSimulationDataGenerator::IdleClockState() const
{
    return mSpiMode == SPI_MODE0 ? CLOCK_LOW : CLOCK_HIGH;
}

uint8_t SpiFlashSimulationDataGenerator::Delay(uint8_t halfClocks)
{
    return halfClocks + HALF_CLOCK_DELAY;
}

SpiFlashSimulationDataGenerator::SpiFlashSimulationDataGenerator()
    : mChipSelectSimulationData(nullptr)
    , mClockSimulationData(nullptr)
    , mMosiSimulationData(nullptr)
    , mMisoSimulationData(nullptr)
    , mD2SimulationData(nullptr)
    , mD3SimulationData(nullptr)
    , mSettings(nullptr)
    , mSimulationSampleRateHz(0)
    , mPendingBitsIx(0)
    , mSpiMode(SPI_MODE0)

{
}

SpiFlashSimulationDataGenerator::~SpiFlashSimulationDataGenerator()
{
}

void SpiFlashSimulationDataGenerator::Initialize(U32 simulation_sample_rate, SpiFlashAnalyzerSettings *settings)
{
    double target_frequency = simulation_sample_rate / 10;
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings               = settings;
    const BusMode initialBusMode = NormalizeBusMode(settings ? settings->mBusMode : SINGLE);
    if (target_frequency > 104000000)
    {
        target_frequency = 104000000;
    }
    mClockGenerator.Init(target_frequency, simulation_sample_rate);
    SetSpiMode(mSettings->mSpiMode == SPI_MODE3 ? SPI_MODE3 : SPI_MODE0);
    spiFlash.SelectCmdSet(spiFlash.GetCommandSet(mSettings->mManufacturer) ? mSettings->mManufacturer : 0);
    spiFlash.SetCurrentCommand(nullptr);
    spiFlash.SetCurrentBusMode(initialBusMode);
    spiFlash.SetDefaultBusMode(initialBusMode);

    if (settings->mChipSelect.mChannelIndex < 1000)
    {
        mChipSelectSimulationData = mSimulationChannels.Add(settings->mChipSelect, mSimulationSampleRateHz, BIT_HIGH);
    }
    else
    {
        mChipSelectSimulationData = nullptr;
    }

    mClockSimulationData = mSimulationChannels.Add(settings->mClock,
                                                   mSimulationSampleRateHz,
                                                   (settings->mSpiMode == 3) ? BIT_HIGH : BIT_LOW);
    mMosiSimulationData  = mSimulationChannels.Add(settings->mMosi, mSimulationSampleRateHz, BIT_HIGH);

    if (settings->mMiso.mChannelIndex < 1000)
    {
        mMisoSimulationData = mSimulationChannels.Add(settings->mMiso, mSimulationSampleRateHz, BIT_HIGH);
    }
    else
    {
        mMisoSimulationData = nullptr;
    }

    if (settings->mD2.mChannelIndex < 1000)
    {
        mD2SimulationData = mSimulationChannels.Add(settings->mD2, mSimulationSampleRateHz, BIT_HIGH);
    }
    else
    {
        mD2SimulationData = nullptr;
    }

    if (settings->mD3.mChannelIndex < 1000)
    {
        mD3SimulationData = mSimulationChannels.Add(settings->mD3, mSimulationSampleRateHz, BIT_HIGH);
    }
    else
    {
        mD3SimulationData = nullptr;
    }

    mSimulationChannels.AdvanceAll(mClockGenerator.AdvanceByHalfPeriod(100));
}

U32 SpiFlashSimulationDataGenerator::GenerateSimulationData(U64 largest_sample_requested,
                                                            U32 sample_rate,
                                                            SimulationChannelDescriptor **simulation_channel)
{
    U64 adjusted_largest_sample_requested =
      AnalyzerHelpers::AdjustSimulationTargetSample(largest_sample_requested, sample_rate, mSimulationSampleRateHz);

    while (mClockSimulationData->GetCurrentSampleNumber() < adjusted_largest_sample_requested)
    {
        GenerateNext();
    }

    *simulation_channel = mSimulationChannels.GetArray();
    return mSimulationChannels.GetCount();
}

void SpiFlashSimulationDataGenerator::GenerateNext()
{
    BusMode mode = spiFlash.GetCurrentBusMode();
    U8 b;
    U8 idleClk = IdleClockState();

    if (mPendingBitsIx >= mPendingBits.size())
    {
        // Add some random delay before new command
        mSimulationChannels.AdvanceAll(mClockGenerator.AdvanceByHalfPeriod(10 + rand() % 10));

        mPendingBits.clear();
        // Set clock in neutral state before CS is activated
        mPendingBits.push_back(CS_HIGH | idleClk);
        // Activate CS
        mPendingBits.push_back(CS_LOW | idleClk);
        // Add some delay
        mPendingBits.push_back(Delay(1));
        // Generate command bits
        GenerateRandomCommandBits(spiFlash, mPendingBits);
        // Make sure clock goes to idle state
        mPendingBits.push_back(CS_LOW | idleClk);
        // Deactivate CS
        mPendingBits.push_back(CS_HIGH | idleClk);
        mPendingBitsIx = 0;
    }

    // Get next bits or delay
    b = mPendingBits[mPendingBitsIx++];
    if (b & HALF_CLOCK_DELAY)
    {
        mSimulationChannels.AdvanceAll(mClockGenerator.AdvanceByHalfPeriod(b & 0x7F));
    }
    else
    {
        // Set all bits and move half period forward
        SetBit(mChipSelectSimulationData, b & CS_HIGH);
        SetBit(mClockSimulationData, b & CLOCK_HIGH);
        SetBit(mMosiSimulationData, b & MOSI_HIGH);
        SetBit(mMisoSimulationData, b & MISO_HIGH);
        SetBit(mD2SimulationData, b & D2_HIGH);
        SetBit(mD3SimulationData, b & D3_HIGH);
        mSimulationChannels.AdvanceAll(mClockGenerator.AdvanceByHalfPeriod(1));
    }
}

void SpiFlashSimulationDataGenerator::GenerateByte(U8 b, std::vector<U8> &bits, BusMode busMode, bool dataIn)
{
    busMode = NormalizeBusMode(busMode);

    // Bits for all the lines at once 0 - CS, 1 - CLK, 2-5 data bits
    U8 lines = 0;
    // Mask for extracting bits x    1b    2b  x    4b
    static const U8 mask[5] = { SPI_FLASH_MASK_NONE,
                                SPI_FLASH_MASK_1B,
                                SPI_FLASH_MASK_2B,
                                SPI_FLASH_MASK_NONE,
                                SPI_FLASH_MASK_4B };
    // row 0 for output, row 1 for input
    // CS is on bit 0
    // CLK on bit 1
    // Data line are on bits 2-5 hence shifts
    static const U8 shift[2][5] = {
        { 0, 5, 4, 0, 2 },
        { 0, 4, 4, 0, 2 }
    };

    for (auto i = 0; i < 8;)
    {
        // Extract as many bits as bus mode wants, they will be left aligned
        lines = b & mask[busMode];
        // shift right to bits 2-5 (for quad), 2-3 (for dual), o 2 or 3 for single mode
        lines >>= shift[dataIn][busMode];

        b <<= busMode;
        i += busMode;
        // Add lines to history, CS is low all the time
        bits.push_back(CS_LOW | lines | CLOCK_LOW);
        // Add same lines with cLock high
        bits.push_back(CS_LOW | lines | CLOCK_HIGH);
    }
}

void SpiFlashSimulationDataGenerator::GenerateCommandBits(SpiFlash &flash, const SpiCmdData *cmd, std::vector<U8> &bits)
{
    int n;
    bool dataIn        = false;
    BusMode curBusMode = NormalizeBusMode(flash.GetCurrentBusMode());
    BusMode defBusMode = NormalizeBusMode(flash.GetDefaultBusMode());
    // Add some delay before CS goes low
    bits.push_back(Delay(SPI_FLASH_DELAY_MIN + rand() % SPI_FLASH_DELAY_RANGE));

    // generate cmd bits if mActiveCmd is not set
    if (flash.GetCurrentCommand() == nullptr)
    {
        GenerateByte(cmd->GetCode(), bits, curBusMode, dataIn);
    }

    // if bus width changes after command code change current bus mode
    if (cmd->mModeArgs)
    {
        curBusMode = NormalizeBusMode(cmd->mModeArgs);
    }

    // generate address
    if (cmd->mAddressBits)
    {
        uint32_t addr   = rand();
        U32 addressBits = (cmd->mAddressBits != SPI_FLASH_ADDR_NOT_SET) ? cmd->mAddressBits : SPI_FLASH_ADDR_BITS_24;
        if (addressBits > SPI_FLASH_ADDR_BITS_24)
        {
            GenerateByte(U8(addr >> 24), bits, curBusMode, dataIn);
        }
        if (addressBits > SPI_FLASH_ADDR_BITS_16)
        {
            GenerateByte(U8(addr >> 16), bits, curBusMode, dataIn);
        }
        if (addressBits > 8)
        {
            GenerateByte(U8(addr >> 8), bits, curBusMode, dataIn);
        }
        GenerateByte(U8(addr), bits, curBusMode, dataIn);
    }

    // for continuous read mode command generate M bits
    if (cmd->mContinuousRead)
    {
        // 3/4 times stay in continuous read mode
        if (rand() % 4)
        {
            GenerateByte(0xAF, bits, curBusMode, dataIn);
        }
        else
        {
            GenerateByte(0xFF, bits, curBusMode, dataIn);
        }
    }

    // Add some dummy bytes if needed
    if (cmd->mDummyBytes)
    {
        for (auto i = 0; i < cmd->mDummyCount; ++i)
        {
            GenerateByte(0xFF, bits, curBusMode, dataIn);
        }
    }
    // or maybe some dummy cycles
    else if (cmd->mDummyCycles)
    {
        for (auto i = 0; i < cmd->mDummyCount; ++i)
        {
            bits.push_back(CS_LOW | MOSI_HIGH | MISO_HIGH | D2_HIGH | D3_HIGH | CLOCK_LOW);
            bits.push_back(CS_LOW | MOSI_HIGH | MISO_HIGH | D2_HIGH | D3_HIGH | CLOCK_HIGH);
        }
    }

    // Switch do other bus mode if this is 1-1-2 or 1-1-4 command
    if (cmd->mModeData)
    {
        curBusMode = NormalizeBusMode(cmd->mModeData);
    }

    // Generate data for commands that have it
    dataIn = (cmd->mCmdOp == OP_REG_READ || cmd->mCmdOp == OP_DATA_READ);
    switch (cmd->mCmdOp)
    {
    case OP_REG_READ:
    case OP_REG_WRITE:
        // For Register read or write just one or to bytes
        n = (int)cmd->RegisterCount();
        break;
    case OP_DATA_READ:
    case OP_DATA_WRITE:
        // For data read or write generate up to 50 bytes
        n = SPI_FLASH_DATA_MIN + rand() % SPI_FLASH_DATA_MAX;
        break;
    default:
        // Commands does not have any additional data
        n = 0;
    }

    // Generate data
    for (auto i = 0; i < n; ++i)
    {
        GenerateByte(U8(rand()), bits, curBusMode, dataIn);
    }

    // If command changed bus mode update it
    // (commands like Enter/Exit QPI mode)
    if (cmd->mModeChange)
    {
        defBusMode = NormalizeBusMode(cmd->mModeChange);
    }

    flash.SetDefaultBusMode(defBusMode);
    flash.SetCurrentBusMode(defBusMode);
}

void SpiFlashSimulationDataGenerator::GenerateRandomCommandBits(SpiFlash &flash, std::vector<U8> &bits)
{
    std::vector<U8> cmds;
    if (flash.GetCurrentCommand())
    {
        GenerateCommandBits(flash, flash.GetCurrentCommand(), bits);
    }
    else
    {
        flash.GetValidCommands(cmds);
        if (cmds.empty())
        {
            return;
        }

        const SpiCmdData *cmd = flash.GetCommand(flash.GetCurrentBusMode(), cmds[rand() % cmds.size()]);
        if (cmd)
            GenerateCommandBits(flash, cmd, bits);
    }
}
