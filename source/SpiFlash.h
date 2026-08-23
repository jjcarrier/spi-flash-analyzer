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

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct BitField
{
    const char* mFieldName;
    uint8_t mUpperBit;
    uint8_t mLowerBit;

    uint32_t GetValue(uint64_t reg) const
    {
        return uint32_t((reg & ((1u << (mUpperBit + 1u)) - 1u)) >> mLowerBit);
    }
};

typedef BitField Bit;

struct RegisterData
{
    const char* mName;
    uint8_t mLen;
    const BitField* mBits;
    uint8_t mBitCount;

    const char* GetName() const
    {
        return mName;
    }

    size_t GetBitfieldCount() const
    {
        return mBitCount;
    }

    const BitField& at(size_t ix) const
    {
        return mBits[ix];
    }
};

enum SpiMode
{
    SPI_MODE0 = 0,
    SPI_MODE3 = 3
};

enum BusMode
{
    UNDEFINED = 0,
    SINGLE    = 1,
    DUAL      = 2,
    QUAD      = 4
};

enum CmdMode
{
    CM_1   = BusMode::SINGLE,
    CM_2   = BusMode::DUAL,
    CM_4   = BusMode::QUAD,
    CM_12  = (BusMode::SINGLE | BusMode::DUAL),
    CM_14  = (BusMode::SINGLE | BusMode::QUAD),
    CM_24  = (BusMode::DUAL | BusMode::QUAD),
    CM_124 = (BusMode::SINGLE | BusMode::DUAL | BusMode::QUAD),
    CM_ALL = CM_124,
};

enum CmdOp
{
    OP_READ,
    OP_WRITE,
    OP_ERASE,
    OP_OTHER
};

enum CmdKind
{
    KIND_DATA,
    KIND_REG,
    KIND_CTRL,
    KIND_OTHER
};

struct SpiCmdData
{
    uint8_t mCode;
    uint8_t mMode;
    CmdKind mCmdKind;
    CmdOp mCmdOp;
    uint8_t mAddressBits;
    uint8_t mModeArgs;
    uint8_t mModeData;
    uint8_t mModeChange;
    uint8_t mDummyCount;
    bool mDummyBytes;
    bool mDummyCycles;
    bool mContinuousRead;
    const char* mNames[4];
    const RegisterData* mRegList[4];

    uint8_t GetCode() const
    {
        return mCode;
    }

    bool IsSingle() const
    {
        return (mMode & CM_1) != 0;
    }

    bool IsDual() const
    {
        return (mMode & CM_2) != 0;
    }

    bool IsQuad() const
    {
        return (mMode & CM_4) != 0;
    }

    bool IsValidForMode(BusMode mode) const
    {
        return (mMode & static_cast<uint8_t>(mode)) != 0;
    }

    bool HasBytePayload() const
    {
        return (mCmdKind == KIND_DATA || mCmdKind == KIND_OTHER) &&
               (mCmdOp == OP_READ || mCmdOp == OP_WRITE);
    }

    bool HasRegisterPayload() const
    {
        return mCmdKind == KIND_REG && (mCmdOp == OP_READ || mCmdOp == OP_WRITE);
    }

    bool IsTabularData() const
    {
        return mCmdKind == KIND_DATA && (mCmdOp == OP_READ || mCmdOp == OP_WRITE);
    }

    size_t NameCount() const
    {
        size_t count = 0;
        while (count < 4 && mNames[count] != nullptr)
        {
            ++count;
        }
        return count;
    }

    bool NamesEmpty() const
    {
        return mNames[0] == nullptr;
    }

    const char* LastName() const
    {
        const size_t count = NameCount();
        return count ? mNames[count - 1] : nullptr;
    }

    const RegisterData* GetRegister(size_t ix) const
    {
        const size_t count = RegisterCount();
        return count ? mRegList[ix % count] : nullptr;
    }

    size_t RegisterCount() const
    {
        size_t count = 0;
        while (count < 4 && mRegList[count] != nullptr)
        {
            ++count;
        }
        return count;
    }
};

struct CmdSet
{
    int mId;
    const char* mName;
    int mParentId;
    const SpiCmdData* mCmds;
    uint16_t mCmdCount;

    int GetId() const
    {
        return mId;
    }

    const char* GetName() const
    {
        return mName;
    }

    const SpiCmdData* FindCommand(BusMode mode, uint8_t code) const;
    const SpiCmdData* GetCommand(BusMode mode, uint8_t code) const;
    void GetValidCommands(BusMode mode, std::vector<uint8_t>& cmds) const;
    void GetContinuousReadCommands(std::vector<const SpiCmdData*>& cmds) const;
};

class SpiFlash
{
    const CmdSet* mActiveCmdSet;
    const SpiCmdData* mCurrentCmd;
    BusMode mCurBusMode;
    BusMode mDefBusMode;
    SpiMode mSpiMode;
    uint32_t mAddressBits;
    bool mDataIn;
    std::vector<const CmdSet*> mCmdSetPtrs;

public:
    SpiFlash();

    void SetDefaultBusMode(BusMode mode)
    {
        mDefBusMode = mode;
    }

    void SetCurrentBusMode(BusMode mode)
    {
        if (mode)
        {
            mCurBusMode = mode;
        }
    }

    BusMode GetCurrentBusMode() const
    {
        return mCurBusMode;
    }

    BusMode GetDefaultBusMode() const
    {
        return mDefBusMode;
    }

    const SpiCmdData* GetCurrentCommand() const
    {
        return mCurrentCmd;
    }

    void SetCurrentCommand(const SpiCmdData* cmd)
    {
        mCurrentCmd = cmd;
    }

    const SpiCmdData* GetCommand(BusMode mode, uint8_t code) const;
    const CmdSet* GetCommandSet(int id) const;
    void SelectCmdSet(int id);
    void GetValidCommands(std::vector<uint8_t>& cmds) const;

    const std::vector<const CmdSet*>& getCommandSets() const
    {
        return mCmdSetPtrs;
    }
};

extern SpiFlash spiFlash;

#endif // SPI_FLASH_H
