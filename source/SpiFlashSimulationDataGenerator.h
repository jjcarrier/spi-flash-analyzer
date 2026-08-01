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

#ifndef SPI_FLASH_SIMULATION_DATA_GENERATOR
#define SPI_FLASH_SIMULATION_DATA_GENERATOR

#include <string>
#include <vector>
#include <SimulationChannelDescriptor.h>
#include <AnalyzerHelpers.h>
#include <AnalyzerTypes.h>
#include "SpiFlash.h"
#include "SpiFlashConstants.h"

class SpiFlashAnalyzerSettings;

class SpiFlashSimulationDataGenerator
{
    // Bit sequence generated for command
    std::vector<U8> mPendingBits;
    // Current index in mPendingBits sequence
    size_t mPendingBitsIx;

    void GenerateNext();

    void SetBit(SimulationChannelDescriptor *channel, U8 high)
    {
        if (channel)
        {
            channel->TransitionIfNeeded(high ? BIT_HIGH : BIT_LOW);
        }
    }

    void SetSpiMode(SpiMode mode);
    uint8_t IdleClockState() const;
    uint8_t Delay(uint8_t halfClocks);

public:
    SpiFlashSimulationDataGenerator();
    ~SpiFlashSimulationDataGenerator();

    void Initialize(U32 simulation_sample_rate, SpiFlashAnalyzerSettings *settings);
    U32 GenerateSimulationData(U64 newest_sample_requested,
                               U32 sample_rate,
                               SimulationChannelDescriptor **simulation_channel);
    void GenerateByte(U8 b, std::vector<U8> &bits, BusMode busMode, bool dataIn);
    void GenerateCommandBits(SpiFlash &flash, const SpiCmdData *cmd, std::vector<U8> &bits);
    void GenerateRandomCommandBits(SpiFlash &flash, std::vector<U8> &bits);

protected:
    ClockGenerator mClockGenerator;
    SpiFlashAnalyzerSettings *mSettings;
    U32 mSimulationSampleRateHz;
    SpiMode mSpiMode;

protected:
    SimulationChannelDescriptorGroup mSimulationChannels;

    SimulationChannelDescriptor *mChipSelectSimulationData;
    SimulationChannelDescriptor *mClockSimulationData;
    SimulationChannelDescriptor *mMosiSimulationData;
    SimulationChannelDescriptor *mMisoSimulationData;
    SimulationChannelDescriptor *mD2SimulationData;
    SimulationChannelDescriptor *mD3SimulationData;
};
#endif // SPI_FLASH_SIMULATION_DATA_GENERATOR
