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

#include "SpiFlashAnalyzerSettings.h"
#include <AnalyzerHelpers.h>
#include "SpiFlash.h"

SpiFlashAnalyzerSettings::SpiFlashAnalyzerSettings()
    : mChipSelect(UNDEFINED_CHANNEL)
    , mClock(UNDEFINED_CHANNEL)
    , mMosi(UNDEFINED_CHANNEL)
    , mMiso(UNDEFINED_CHANNEL)
    , mD2(UNDEFINED_CHANNEL)
    , mD3(UNDEFINED_CHANNEL)
    , mManufacturer(0)
    , mAddressLength(SPI_FLASH_ADDR_BITS_24)
    , mSpiMode(SPI_FLASH_CMD_NOT_SET)
    , mBusMode(1)
    , mEnableSampleMarkers(false)
    , mEnableCommandSummary(false)
    , mIncludeWREN(true)
    , mIncludeRDSR(true)
    , mContinuousRead(0)
{
    mChipSelectInterface.reset(new AnalyzerSettingInterfaceChannel());
    mChipSelectInterface->SetTitleAndTooltip("CS", "Select Chip select line");
    mChipSelectInterface->SetChannel(mChipSelect);

    mClockInterface.reset(new AnalyzerSettingInterfaceChannel());
    mClockInterface->SetTitleAndTooltip("Clock", "Select Clock line");
    mClockInterface->SetChannel(mClock);

    mMosiInterface.reset(new AnalyzerSettingInterfaceChannel());
    mMosiInterface->SetTitleAndTooltip("MOSI", "Select MOSI line");
    mMosiInterface->SetChannel(mMosi);

    mMisoInterface.reset(new AnalyzerSettingInterfaceChannel());
    mMisoInterface->SetTitleAndTooltip("MISO", "Select MISO line");
    mMisoInterface->SetChannel(mMiso);

    mD2Interface.reset(new AnalyzerSettingInterfaceChannel());
    mD2Interface->SetTitleAndTooltip("D2", "Select D2 line");
    mD2Interface->SetSelectionOfNoneIsAllowed(true);
    mD2Interface->SetChannel(mD2);

    mD3Interface.reset(new AnalyzerSettingInterfaceChannel());
    mD3Interface->SetTitleAndTooltip("D3", "Select D3 line");
    mD3Interface->SetSelectionOfNoneIsAllowed(true);
    mD3Interface->SetChannel(mD3);

    mManufacturerInterface.reset(new AnalyzerSettingInterfaceNumberList());
    mManufacturerInterface->SetTitleAndTooltip("Manufacturer", "Select flash manufacturer");
    for (size_t i = 0; i < spiFlash.getCommandSets().size(); ++i)
        mManufacturerInterface->AddNumber(spiFlash.getCommandSets()[i]->GetId(),
                                          spiFlash.getCommandSets()[i]->GetName(),
                                          "");

    mManufacturerInterface->SetNumber(mManufacturer);
    spiFlash.SelectCmdSet(mManufacturer);

    mAddressLengthInterface.reset(new AnalyzerSettingInterfaceNumberList());
    mAddressLengthInterface->SetTitleAndTooltip("Address", "Select address length");
    mAddressLengthInterface->AddNumber(SPI_FLASH_ADDR_BITS_24, "24 bits", "");
    mAddressLengthInterface->AddNumber(SPI_FLASH_ADDR_BITS_16, "16 bits", "");
    mAddressLengthInterface->AddNumber(SPI_FLASH_ADDR_BITS_8, "8 bits", "");
    // TODO: Restore 32 bits
    // mAddressLengthInterface->AddNumber(32, "32 bits", "");
    mAddressLengthInterface->SetNumber(mAddressLength);

    mSpiModeInterface.reset(new AnalyzerSettingInterfaceNumberList());
    mSpiModeInterface->SetTitleAndTooltip("SPI Mode", "Select SPI mode");
    mSpiModeInterface->AddNumber(0xFF, "Auto", "");
    mSpiModeInterface->AddNumber(0, "SPI Mode 0", "");
    mSpiModeInterface->AddNumber(3, "SPI Mode 3", "");
    mSpiModeInterface->SetNumber(mSpiMode);

    mBusModeInterface.reset(new AnalyzerSettingInterfaceNumberList());
    mBusModeInterface->SetTitleAndTooltip("Start in", "Number of lines used for command transmission");
    mBusModeInterface->AddNumber(1, "Single", "");
    mBusModeInterface->AddNumber(2, "Dual", "");
    mBusModeInterface->AddNumber(4, "Quad", "");
    mBusModeInterface->SetNumber(mBusMode);

    mContinuousReadInterface.reset(new AnalyzerSettingInterfaceNumberList());
    mContinuousReadInterface->SetTitleAndTooltip("Continuous read",
                                                 "Command that is continuously read at start of analysis");
    mContinuousReadInterface->AddNumber(0, "Disabled (Command Mode)", "");
    for (size_t i = 0; i < spiFlash.getCommandSets().size(); ++i)
    {
        const CmdSet *cmdSet = spiFlash.getCommandSets()[i];
        std::vector<const SpiCmdData *> continuousReadCmds;
        cmdSet->GetContinuousReadCommands(continuousReadCmds);

        for (size_t j = 0; j < continuousReadCmds.size(); ++j)
        {
            char t[100];
            const SpiCmdData *cmd = continuousReadCmds[j];
            snprintf(t, 100, "0x%02X: %s (%s)", cmd->GetCode(), cmd->LastName(), cmdSet->GetName());
            mContinuousReadInterface->AddNumber((cmdSet->GetId() << 8) + cmd->GetCode(), t, "");
        }
    }
    mContinuousReadInterface->SetNumber(0);

    mEnableSampleMarkersInterface.reset(new AnalyzerSettingInterfaceBool());
    mEnableSampleMarkersInterface->SetCheckBoxText("Show sample-point markers");
    mEnableSampleMarkersInterface->SetValue(mEnableSampleMarkers);


    mEnableCommandSummaryInterface.reset(new AnalyzerSettingInterfaceBool());
    mEnableCommandSummaryInterface->SetCheckBoxText("Show command summary on CS");
    mEnableCommandSummaryInterface->SetValue(mEnableCommandSummary);

    // Add checkboxes for WREN and RDSR V2 frame inclusion
    mIncludeWRENInterface.reset(new AnalyzerSettingInterfaceBool());
    mIncludeWRENInterface->SetCheckBoxText("Include WREN instructions in V2 frame results");
    mIncludeWRENInterface->SetValue(mIncludeWREN);

    mIncludeRDSRInterface.reset(new AnalyzerSettingInterfaceBool());
    mIncludeRDSRInterface->SetCheckBoxText("Include RDSR instructions in V2 frame results");
    mIncludeRDSRInterface->SetValue(mIncludeRDSR);

    AddInterface(mChipSelectInterface.get());
    AddInterface(mClockInterface.get());
    AddInterface(mMosiInterface.get());
    AddInterface(mMisoInterface.get());
    AddInterface(mD2Interface.get());
    AddInterface(mD3Interface.get());

    AddInterface(mManufacturerInterface.get());
    AddInterface(mAddressLengthInterface.get());
    AddInterface(mSpiModeInterface.get());
    AddInterface(mBusModeInterface.get());
    AddInterface(mContinuousReadInterface.get());
    AddInterface(mEnableSampleMarkersInterface.get());
    AddInterface(mEnableCommandSummaryInterface.get());
    AddInterface(mIncludeWRENInterface.get());
    AddInterface(mIncludeRDSRInterface.get());

    AddExportOption(0, "Export as text/csv file");
    AddExportExtension(0, "text", "txt");
    AddExportExtension(0, "csv", "csv");

    ClearChannels();

    AddChannel(mChipSelect, "Chip Select", false);
    AddChannel(mClock, "Clock", false);
    AddChannel(mMosi, "MOSI", false);
    AddChannel(mMiso, "MISO", false);
    AddChannel(mD2, "D2", false);
    AddChannel(mD3, "D3", false);
}

SpiFlashAnalyzerSettings::~SpiFlashAnalyzerSettings()
{
}

bool SpiFlashAnalyzerSettings::SetSettingsFromInterfaces()
{
    mManufacturer   = U32(mManufacturerInterface->GetNumber());
    mAddressLength  = U32(mAddressLengthInterface->GetNumber());
    mSpiMode        = U32(mSpiModeInterface->GetNumber());
    mBusMode        = U32(mBusModeInterface->GetNumber());
    mContinuousRead = U32(mContinuousReadInterface->GetNumber());
    mChipSelect     = mChipSelectInterface->GetChannel();
    mClock          = mClockInterface->GetChannel();
    mMosi           = mMosiInterface->GetChannel();
    mMiso           = mMisoInterface->GetChannel();
    mD2             = mD2Interface->GetChannel();
    mD3             = mD3Interface->GetChannel();
    mEnableSampleMarkers = mEnableSampleMarkersInterface->GetValue();
    mEnableCommandSummary = mEnableCommandSummaryInterface->GetValue();
    mIncludeWREN    = mIncludeWRENInterface->GetValue();
    mIncludeRDSR    = mIncludeRDSRInterface->GetValue();

    ClearChannels();

    AddChannel(mChipSelect, "Chip Select", true);
    AddChannel(mClock, "Clock", true);
    AddChannel(mMosi, "MOSI", true);
    AddChannel(mMiso, "MISO", true);
    AddChannel(mD2, "D2", true);
    AddChannel(mD3, "D3", true);

    spiFlash.SelectCmdSet(mManufacturer);

    return true;
}

void SpiFlashAnalyzerSettings::UpdateInterfacesFromSettings()
{
    mManufacturerInterface->SetNumber(mManufacturer);
    mAddressLengthInterface->SetNumber(mAddressLength);
    mSpiModeInterface->SetNumber(mSpiMode);
    mBusModeInterface->SetNumber(mBusMode);
    mContinuousReadInterface->SetNumber(mContinuousRead);
    mChipSelectInterface->SetChannel(mChipSelect);
    mClockInterface->SetChannel(mClock);
    mMosiInterface->SetChannel(mMosi);
    mMisoInterface->SetChannel(mMiso);
    mD2Interface->SetChannel(mD2);
    mD3Interface->SetChannel(mD3);
    mEnableSampleMarkersInterface->SetValue(mEnableSampleMarkers);
    mEnableCommandSummaryInterface->SetValue(mEnableCommandSummary);
    mIncludeWRENInterface->SetValue(mIncludeWREN);
    mIncludeRDSRInterface->SetValue(mIncludeRDSR);
}

void SpiFlashAnalyzerSettings::LoadSettings(const char *settings)
{
    SimpleArchive text_archive;
    text_archive.SetString(settings);

    text_archive >> mManufacturer;
    text_archive >> mAddressLength;
    text_archive >> mSpiMode;
    text_archive >> mBusMode;
    text_archive >> mContinuousRead;
    text_archive >> mChipSelect;
    text_archive >> mClock;
    text_archive >> mMosi;
    text_archive >> mMiso;
    text_archive >> mD2;
    text_archive >> mD3;
    text_archive >> mIncludeWREN;
    text_archive >> mIncludeRDSR;
    text_archive >> mEnableSampleMarkers;
    text_archive >> mEnableCommandSummary;

    ClearChannels();
    AddChannel(mChipSelect, "Chip Select", true);
    AddChannel(mClock, "Clock", true);
    AddChannel(mMosi, "MOSI", true);
    AddChannel(mMiso, "MISO", true);
    AddChannel(mD2, "D2", true);
    AddChannel(mD3, "D3", true);

    UpdateInterfacesFromSettings();
}

const char *SpiFlashAnalyzerSettings::SaveSettings()
{
    SimpleArchive text_archive;

    text_archive << mManufacturer;
    text_archive << mAddressLength;
    text_archive << mSpiMode;
    text_archive << mBusMode;
    text_archive << mContinuousRead;
    text_archive << mChipSelect;
    text_archive << mClock;
    text_archive << mMosi;
    text_archive << mMiso;
    text_archive << mD2;
    text_archive << mD3;
    text_archive << mIncludeWREN;
    text_archive << mIncludeRDSR;
    text_archive << mEnableSampleMarkers;
    text_archive << mEnableCommandSummary;

    return SetReturnString(text_archive.GetString());
}
