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

#include "SpiFlashAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "SpiFlashAnalyzer.h"
#include "SpiFlashAnalyzerSettings.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>

#include "SpiFlash.h"
#include "SpiFlashConstants.h"

SpiFlashAnalyzerResults::SpiFlashAnalyzerResults(SpiFlashAnalyzer *analyzer, SpiFlashAnalyzerSettings *settings)
    : AnalyzerResults(), mSettings(settings), mAnalyzer(analyzer)
{
}

SpiFlashAnalyzerResults::~SpiFlashAnalyzerResults()
{
}

static int AddressBits(U32 addr)
{
    if (addr < SPI_FLASH_ADDR_8_MAX)
        return SPI_FLASH_ADDR_BITS_8;
    else if (addr < SPI_FLASH_ADDR_16_MAX)
        return SPI_FLASH_ADDR_BITS_16;
    else if (addr < SPI_FLASH_ADDR_24_MAX)
        return SPI_FLASH_ADDR_BITS_24;
    else
        return SPI_FLASH_ADDR_BITS_32;
}

static std::string RegisterString(const RegisterData *reg, U64 val, bool full = false)
{
    std::stringstream s;

    for (size_t i = 0; i < reg->GetBitfieldCount(); ++i)
    {
        const BitField &bitField = reg->at(i);
        U32 bitsValue            = bitField.GetValue(val);
        if (bitsValue || full)
        {
            s << (s.tellp() ? " " : "") << bitField.mFieldName << "=" << std::hex << bitsValue;
        }
    }
    return s.str();
}

static void FormatHexByte(U8 value, char *buffer, size_t buffer_size)
{
    std::snprintf(buffer, buffer_size, "%02X", value);
}

static void FormatHexAddress(U32 value, char *buffer, size_t buffer_size)
{
    std::snprintf(buffer, buffer_size, "%X", value);
}

static bool IsPrintableAscii(U8 value)
{
    return value >= 0x20 && value <= 0x7E;
}

static const char *TabularDataPrefix(U32 offset)
{
    if (offset == 0) return "\t";

    return (offset % 16u) == 0 ? "\n\t" : " ";
}

static std::string BuildTabularHexRegion(const U8 (&values)[16], const bool (&present)[16])
{
    std::string hex_region;
    hex_region.reserve((16u * 3u) - 1u);

    char number_str[3];
    for (U32 i = 0; i < 16; ++i)
    {
        if (i != 0)
        {
            hex_region += ' ';
        }

        if (present[i])
        {
            FormatHexByte(values[i], number_str, sizeof(number_str));
            hex_region += number_str;
        }
        else
        {
            hex_region += "  ";
        }
    }

    return hex_region;
}

static std::string BuildTabularAsciiRegion(const U8 (&values)[16], const bool (&present)[16])
{
    std::string ascii_region(16, ' ');

    for (U32 i = 0; i < 16; ++i)
    {
        if (present[i])
        {
            ascii_region[i] = IsPrintableAscii(values[i]) ? char(values[i]) : '.';
        }
    }

    return ascii_region;
}

static bool GetTabularDataOperation(const SpiCmdData *cmd, const char *&operation)
{
    if (cmd == nullptr)
    {
        return false;
    }

    // TODO: track packet state to filter out non-flash-data based output
    //if (!cmd->IsTabularData())
    //{
    //    return false;
    //}

    if (cmd->mCmdOp == OP_READ)
    {
        operation = "RD";
        return true;
    }

    if (cmd->mCmdOp == OP_WRITE)
    {
        operation = "WR";
        return true;
    }

    return false;
}

static bool GetTabularPayloadByte(const Frame &frame, U32 &offset, U8 &value)
{
    if (frame.mType == FT_OUT_BYTE)
    {
        U64 packed = frame.mData1;
        offset     = U32(packed >> 8);
        value      = U8(packed & 0xFF);
        return true;
    }

    if (frame.mType == FT_IN_BYTE)
    {
        U64 packed = frame.mData2;
        offset     = U32(packed >> 8);
        value      = U8(packed & 0xFF);
        return true;
    }

    return false;
}

static bool IsLastTabularPayloadByte(SpiFlashAnalyzerResults *results, U64 frame_index, U32 offset)
{
    const U64 num_frames = results->GetNumFrames();
    for (U64 i = frame_index + 1; i < num_frames; ++i)
    {
        Frame next_frame = results->GetFrame(i);

        U32 next_offset = 0;
        U8 next_value   = 0;
        if (GetTabularPayloadByte(next_frame, next_offset, next_value))
        {
            return next_offset != (offset + 1u);
        }

        if (next_frame.mType == FT_CMD)
        {
            return true;
        }
    }

    return true;
}

static void CollectCompletedTabularLineBytes(SpiFlashAnalyzerResults *results,
                                             U64 frame_index,
                                             U32 line_start_offset,
                                             U8 (&values)[16],
                                             bool (&present)[16])
{
    for (S64 i = S64(frame_index); i >= 0; --i)
    {
        U32 offset = 0;
        U8 value   = 0;
        if (!GetTabularPayloadByte(results->GetFrame(U64(i)), offset, value))
        {
            if (U64(i) != frame_index)
            {
                break;
            }

            continue;
        }

        if (offset < line_start_offset)
        {
            break;
        }

        const U32 slot = offset - line_start_offset;
        if (slot < 16u)
        {
            values[slot]  = value;
            present[slot] = true;
        }

        if (offset == line_start_offset)
        {
            break;
        }
    }
}

static bool FindTabularTransactionHeader(SpiFlashAnalyzerResults *results,
                                         U64 frame_index,
                                         const char *&operation,
                                         U32 &address,
                                         bool &has_address)
{
    operation   = nullptr;
    has_address = false;
    address     = 0;

    for (S64 i = S64(frame_index); i >= 0; --i)
    {
        Frame frame = results->GetFrame(U64(i));

        if (!has_address && (frame.mType == FT_OUT_ADDR24 || frame.mType == FT_OUT_ADDR32))
        {
            address     = U32(frame.mData1);
            has_address = true;
        }

        if (frame.mType == FT_CMD_BYTE)
        {
            const SpiCmdData *cmd = reinterpret_cast<const SpiCmdData *>(frame.mData2);
            return U64(cmd) > SPI_FLASH_INVALID_CMD && GetTabularDataOperation(cmd, operation);
        }

        if (frame.mType == FT_CMD)
        {
            const SpiCmdData *cmd = reinterpret_cast<const SpiCmdData *>(frame.mData2);
            if (U64(cmd) > SPI_FLASH_INVALID_CMD && GetTabularDataOperation(cmd, operation))
            {
                if (!has_address && cmd->mAddressBits)
                {
                    address     = U32(frame.mData1 >> 24);
                    has_address = true;
                }

                return true;
            }

            return false;
        }
    }

    return false;
}

void SpiFlashAnalyzerResults::AddRegisterResult(const RegisterData *reg, U64 val, DisplayBase display_base)
{
    char number_str[SPI_FLASH_NUMSTR_BUF_SIZE];
    AnalyzerHelpers::GetNumberString(val, display_base, SPI_FLASH_DATA_BITS, number_str, SPI_FLASH_NUMSTR_BUF_SIZE);
    AddResultString(number_str);
    // There is register assigned
    if (reg)
    {
        std::string s = RegisterString(reg, val);
        if (s.size())
        {
            AddResult(s);
        }

        AddResult(RegisterString(reg, val, true));
    }
}

void SpiFlashAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel &channel, DisplayBase display_base)
{
    ClearResultStrings();
    Frame frame = GetFrame(frame_index);

    char number_str[SPI_FLASH_NUMSTR_BUF_SIZE];
    std::stringstream fulls, shorts;
    if (mSettings->mEnableCommandSummary && frame.mType == FT_CMD &&
        ((mSettings->mChipSelect != UNDEFINED_CHANNEL && channel == mSettings->mChipSelect)))
    {
        char number_str2[10];
        const SpiCmdData *cmd = reinterpret_cast<const SpiCmdData *>(frame.mData2);
        if (U64(cmd) > SPI_FLASH_INVALID_CMD)
        {
            const char *s[4] = { 0 };
            for (size_t i = 0; i < cmd->NameCount(); ++i)
            {
                AddResultString(cmd->mNames[i]);
            }

            if (cmd->mAddressBits)
            {
                U32 addr = U32(frame.mData1 >> 24);
                s[0]     = "  A=";
                s[1]     = number_str;
                AnalyzerHelpers::GetNumberString(addr, Hexadecimal, AddressBits(addr), number_str, SPI_FLASH_NUMSTR_BUF_SIZE);
            }

            if (cmd->HasBytePayload())
            {
                s[2] = "  Bytes:";
                s[3] = number_str2;
                AnalyzerHelpers::GetNumberString(frame.mData1 & 0xFFFFFF,
                                                 Decimal,
                                                 SPI_FLASH_ADDR_BITS_24,
                                                 number_str2,
                                                 SPI_FLASH_NUMSTR_BUF_SIZE);
            }
            // Add longest name with address and byte count if present
            AddResultString(cmd->LastName(), s[0], s[1], s[2], s[3]);
        }
        else
        {
            AddResultString("?");
            if (frame.mData2 != SPI_FLASH_INVALID_CMD)
            {
                AnalyzerHelpers::GetNumberString(frame.mData2,
                                                 display_base,
                                                 SPI_FLASH_CMD_BITS,
                                                 number_str,
                                                 SPI_FLASH_NUMSTR_BUF_SIZE);
                AddResultString("? [", number_str, "]");
            }
        }
    }
    else if (frame.mType == FT_CMD_BYTE && channel == mSettings->mMosi)
    {
        const SpiCmdData *cmd = reinterpret_cast<const SpiCmdData *>(frame.mData2);
        if (frame.mData2 == 0x100)
        {
            AddResultString("?"); // Not enough bits
        }
        else if (frame.mData2 < SPI_FLASH_INVALID_CMD)
        {
            // Normal byte and CMD=0xXX
            AnalyzerHelpers::GetNumberString(frame.mData2,
                                             display_base,
                                             SPI_FLASH_CMD_BITS,
                                             number_str,
                                             SPI_FLASH_NUMSTR_BUF_SIZE);
            AddResultString(number_str);
            AnalyzerHelpers::GetNumberString(frame.mData2,
                                             Hexadecimal,
                                             SPI_FLASH_CMD_BITS,
                                             number_str,
                                             SPI_FLASH_NUMSTR_BUF_SIZE);
            AddResultString("[", number_str, "]");
        }
        else
        {
            AnalyzerHelpers::GetNumberString(cmd->GetCode(),
                                             Hexadecimal,
                                             SPI_FLASH_CMD_BITS,
                                             number_str,
                                             SPI_FLASH_NUMSTR_BUF_SIZE);
            for (size_t i = 0; i < cmd->NameCount(); ++i)
            {
                AddResultString(cmd->mNames[i]);
            }
            AddResultString(cmd->LastName(), " [", number_str, "]");
        }
    }
    else if (frame.mType == FT_OUT_ADDR24 && channel == mSettings->mMosi)
    {
        AnalyzerHelpers::GetNumberString(frame.mData1,
                                         Hexadecimal,
                                         AddressBits(U32(frame.mData1 >> 24)),
                                         number_str,
                                         SPI_FLASH_NUMSTR_BUF_SIZE);
        AddResultString(number_str);
        AddResultString("A=", number_str);
    }
    else if ((frame.mType == FT_OUT_BYTE || frame.mType == FT_IN_OUT) && channel == mSettings->mMosi)
    {
        U32 offset = U32(frame.mData1 >> 8);
        U8 mosi    = U8(frame.mData1 & 0xFF);
        AnalyzerHelpers::GetNumberString(mosi,
                                         display_base,
                                         SPI_FLASH_DATA_BITS,
                                         number_str,
                                         SPI_FLASH_NUMSTR_BUF_SIZE);
        char offset_str[SPI_FLASH_OFFSETSTR_BUF_SIZE];
        AnalyzerHelpers::GetNumberString(offset,
                                         Decimal,
                                         SPI_FLASH_DATA_OFFSET_BITS,
                                         offset_str,
                                         SPI_FLASH_OFFSETSTR_BUF_SIZE);
        AddResultString(number_str);
        AddResultString(number_str, " [Byte ", offset_str, "]");
    }
    else if (frame.mType == FT_IN_REG && channel == mSettings->mMiso)
    {
        AddRegisterResult(reinterpret_cast<const RegisterData *>(frame.mData1), frame.mData2, display_base);
    }
    else if (frame.mType == FT_OUT_REG && channel == mSettings->mMosi)
    {
        AddRegisterResult(reinterpret_cast<const RegisterData *>(frame.mData2), frame.mData1, display_base);
    }
    else if ((frame.mType == FT_M) && channel == mSettings->mMosi)
    {
        AddResultString("M");
        AnalyzerHelpers::GetNumberString(frame.mData1,
                                         Hexadecimal,
                                         SPI_FLASH_MODE_BITS,
                                         number_str,
                                         SPI_FLASH_NUMSTR_BUF_SIZE);
        AddResultString(number_str);
        AddResultString("M=", number_str);
    }
    else if ((frame.mType == FT_IN_BYTE || frame.mType == FT_IN_OUT) && channel == mSettings->mMiso)
    {
        U32 offset = U32(frame.mData2 >> 8);
        U8 miso    = U8(frame.mData2 & 0xFF);
        AnalyzerHelpers::GetNumberString(miso,
                                         display_base,
                                         SPI_FLASH_DATA_BITS,
                                         number_str,
                                         SPI_FLASH_NUMSTR_BUF_SIZE);
        char offset_str[SPI_FLASH_OFFSETSTR_BUF_SIZE];
        AnalyzerHelpers::GetNumberString(offset,
                                         Decimal,
                                         SPI_FLASH_DATA_OFFSET_BITS,
                                         offset_str,
                                         SPI_FLASH_OFFSETSTR_BUF_SIZE);
        AddResultString(number_str);
        AddResultString(number_str, " [Byte ", offset_str, "]");
    }
    else if (frame.mType == FT_DUMMY && channel == mSettings->mMosi)
    {
        AddResultString("X");
        AddResultString("Dummy");
    }
}

void SpiFlashAnalyzerResults::GenerateExportFile(const char *file, DisplayBase display_base, U32 export_type_user_id)
{
    std::ofstream file_stream(file, std::ios::out);

    U64 trigger_sample = mAnalyzer->GetTriggerSample();
    U32 sample_rate    = mAnalyzer->GetSampleRate();

    file_stream << "Time [s],Value" << '\n';

    U64 num_frames = GetNumFrames();
    for (U32 i = 0; i < num_frames; i++)
    {
        Frame frame = GetFrame(i);

        char time_str[128];
        AnalyzerHelpers::GetTimeString(frame.mStartingSampleInclusive,
                                       trigger_sample,
                                       sample_rate,
                                       time_str,
                                       sizeof(time_str));

        char number_str[128];
        AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 8, number_str, sizeof(number_str));

        file_stream << time_str << "," << number_str << '\n';

        if (UpdateExportProgressAndCheckForCancel(i, num_frames) == true)
        {
            file_stream.close();
            return;
        }
    }

    file_stream.close();
}

void SpiFlashAnalyzerResults::GenerateFrameTabularText(U64 frame_index, DisplayBase display_base)
{
    ClearTabularText();
    Frame frame = GetFrame(frame_index);

    char number_str[128];
    U32 offset = 0;
    U8 value   = 0;
    if (GetTabularPayloadByte(frame, offset, value))
    {
        if ((offset % 16u) != 15u && !IsLastTabularPayloadByte(this, frame_index, offset))
        {
            return;
        }

        const U32 line_start_offset = offset - (offset % 16u);
        std::string prefix = TabularDataPrefix(line_start_offset);
        if (line_start_offset == 0)
        {
            const char *operation = nullptr;
            U32 address           = 0;
            bool has_address      = false;
            if (FindTabularTransactionHeader(this, frame_index, operation, address, has_address))
            {
                if (has_address)
                {
                    FormatHexAddress(address, number_str, sizeof(number_str));
                    prefix = std::string("\n") + operation + " @ 0x" + number_str + ":\n\t";
                }
                else
                {
                    prefix = std::string("\n") + operation + ":\n\t";
                }
            }
        }

        U8 line_values[16]   = {};
        bool line_present[16] = {};
        CollectCompletedTabularLineBytes(this, frame_index, line_start_offset, line_values, line_present);

        const std::string hex_region   = BuildTabularHexRegion(line_values, line_present);
        const std::string ascii_region = BuildTabularAsciiRegion(line_values, line_present);
        AddTabularText(prefix.c_str(), hex_region.c_str(), "  ", ascii_region.c_str());
    }
}

void SpiFlashAnalyzerResults::GeneratePacketTabularText(U64 packet_id, DisplayBase display_base)
{
    ClearResultStrings();
    AddResultString("not supported");
}

void SpiFlashAnalyzerResults::GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base)
{
    ClearResultStrings();
    AddResultString("not supported");
}
