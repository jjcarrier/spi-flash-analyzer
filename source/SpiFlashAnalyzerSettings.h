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

#ifndef SPI_FLASH_ANALYZER_SETTINGS
#define SPI_FLASH_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>
#include "SpiFlashConstants.h"

class SpiFlashAnalyzerSettings : public AnalyzerSettings
{
public:
    SpiFlashAnalyzerSettings();
    virtual ~SpiFlashAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    void UpdateInterfacesFromSettings();
    virtual void LoadSettings(const char* settings);
    virtual const char* SaveSettings();

    Channel mChipSelect;
    Channel mClock;
    Channel mMosi;
    Channel mMiso;
    Channel mD2;
    Channel mD3;
    U32 mManufacturer;
    U32 mAddressLength;
    U32 mSpiMode;
    U32 mBusMode;
    U32 mContinuousRead;

    // V2 Frame Filtering Options
    bool mEnableSampleMarkers;
    bool mEnableCommandSummary;
    bool mIncludeWREN;
    bool mIncludeRDSR;

protected:
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mManufacturerInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mAddressLengthInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mSpiModeInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mBusModeInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mContinuousReadInterface;

    std::unique_ptr<AnalyzerSettingInterfaceChannel> mChipSelectInterface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mClockInterface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mMosiInterface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mMisoInterface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mD2Interface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mD3Interface;

    // New interface pointers for checkboxes
    std::unique_ptr<AnalyzerSettingInterfaceBool> mEnableSampleMarkersInterface;
    std::unique_ptr<AnalyzerSettingInterfaceBool> mEnableCommandSummaryInterface;
    std::unique_ptr<AnalyzerSettingInterfaceBool> mIncludeWRENInterface;
    std::unique_ptr<AnalyzerSettingInterfaceBool> mIncludeRDSRInterface;
};

#endif // SPI_FLASH_ANALYZER_SETTINGS
