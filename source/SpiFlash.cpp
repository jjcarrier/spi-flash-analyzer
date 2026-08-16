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

#include "SpiFlash.h"
#include "SpiFlashConstants.h"
#include <algorithm>
#include <cstddef>

// ---------------------------------------------------------------------------
// Static read-only tables.
//
// Data is organized manufacturer by manufacturer:
//   1. BitField arrays for each register
//   2. RegisterData objects referencing those arrays
//   3. SpiCmdData arrays for the command set
//   4. The kCmdSets master table
//
// SpiCmdData aggregate-init column order:
//   { code, mode, cmdOp, addrBits, modeArgs, modeData, modeChange,
//     dummyCount, dummyBytes, dummyCycles, contRead,
//     {name0, name1, name2, name3},
//     {reg0,  reg1,  reg2,  reg3 } }
//
//   addrBits: 0=none, 0xFF=use configured address width, 16=explicit 16-bit.
//   modeArgs: address/mode-byte phase I/O width (0=single, 2=dual, 4=quad).
//   modeData: data phase I/O width (0=single, 2=dual, 4=quad).
//   modeChange: switch bus mode after command (0=none, CM_1, CM_4).
// ---------------------------------------------------------------------------

static constexpr uint8_t kAddrGlobal = 0xFF; // use configured address width
static constexpr uint8_t kAddr16     = 16;   // explicit 16-bit address (ADDR2)

// ===========================================================================
// COMMON (id = 0)
// ===========================================================================

static const BitField kBits_SR1_Common[] = {
    { "SRP0", 7, 7 },
    { "WEL",  1, 1 },
    { "BUSY", 0, 0 }
};
static const BitField kBits_SR2_Common[] = {
    { "SUS",  7, 7 },
    { "QE",   1, 1 },
    { "SRP1", 0, 0 }
};
static const RegisterData kSR1_Common = { "SR1", 8, kBits_SR1_Common, 3 };
static const RegisterData kSR2_Common = { "SR2", 8, kBits_SR2_Common, 3 };

static const SpiCmdData kCmds_Common[] = {
    /* mCode, mMode, mCmdOp, mAddressBits, mModeArgs, mModeData, mModeChange, mDummyCount, mDummyBytes, mDummyCycles, mContinuousRead, nNames[], mRegList[] */
    // Read commands
    { 0x05, CM_14, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR", "Read SR1", nullptr, nullptr },                     { &kSR1_Common, nullptr, nullptr, nullptr }      },
    { 0x35, CM_14, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR2", "Read SR2", nullptr, nullptr },                    { &kSR2_Common, nullptr, nullptr, nullptr }      },
    { 0x03, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 0, false, false, false, { "RD", "Read Data", nullptr, nullptr },                      { nullptr, nullptr, nullptr, nullptr }           },
    { 0x0B, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "FRD", "Fast Read", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr }           },
    { 0x3B, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 2, 0, 1, true,  false, false, { "DRD", "Fast Read Dual Output", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }           },
    { 0x6B, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 4, 0, 1, true,  false, false, { "QRD", "Fast Read Quad Output", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }           },
    { 0xBB, CM_1,  OP_DATA_READ,  kAddrGlobal, 2, 0, 0, 0, false, false, true,  { "DRIO", "Fast Read Dual I/O", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }           },
    { 0xEB, CM_1,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0, 2, true,  false, true,  { "QRIO", "Fast Read Quad I/O", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }           },
    { 0x5A, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "SFDP", "Read SFDP Register", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }           },
    { 0x9F, CM_14, OP_DATA_READ,  0,           0, 0, 0, 0, false, false, false, { "JID", "Read JEDEC ID", nullptr, nullptr },                 { nullptr, nullptr, nullptr, nullptr }           },
    { 0x90, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 0, false, false, false, { "MFID", "Read Manufacturer, Device ID", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }           },
    // Write / control
    { 0x06, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WREN", "Write Enable", nullptr, nullptr },                 { nullptr, nullptr, nullptr, nullptr }           },
    { 0x04, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WRDI", "Write Disable", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr }           },
    { 0x01, CM_14, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR", "Write SR", nullptr, nullptr },                     { &kSR1_Common, &kSR2_Common, nullptr, nullptr } },
    { 0x31, CM_14, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR2", "Write SR2", nullptr, nullptr },                   { &kSR2_Common, nullptr, nullptr, nullptr }      },
    { 0x02, CM_14, OP_DATA_WRITE, kAddrGlobal, 0, 0, 0, 0, false, false, false, { "PP", "Page Program", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }           },
    // Erase
    { 0x20, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "SE", "SE4", "4kB Sector Erase", nullptr },                 { nullptr, nullptr, nullptr, nullptr }           },
    { 0x52, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "BE", "BE32", "32kB Block Erase", nullptr },                { nullptr, nullptr, nullptr, nullptr }           },
    { 0xD8, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "BE", "BE64", "64kB Block Erase", nullptr },                { nullptr, nullptr, nullptr, nullptr }           },
    { 0x60, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "CE", "Chip Erase", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr }           },
    { 0xC7, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "CE", "Chip Erase", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr }           },
    // Misc
    { 0x75, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "SUSP", "Erase/Program Suspend", nullptr, nullptr },        { nullptr, nullptr, nullptr, nullptr }           },
    { 0x7A, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RESM", "Erase/Program Resume", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }           },
    { 0x66, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RSTEN", "Enable Reset", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr }           },
    { 0x99, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RST", "Reset", nullptr, nullptr },                         { nullptr, nullptr, nullptr, nullptr }           },
    { 0xB9, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "DN", "Power Down", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr }           },
    { 0xAB, CM_14, OP_DATA_READ,  0,           0, 0, 0, 3, true,  false, false, { "UP", "Release Power Down", nullptr, nullptr },             { nullptr, nullptr, nullptr, nullptr }           },
};

// ===========================================================================
// RENESAS (id = 0x1F, parent = 0)
// ===========================================================================

static const BitField kBits_SR1_Renesas[] = {
    { "SRP0", 7, 7 },
    { "BPB",  6, 2 },
    { "WEL",  1, 1 },
    { "BUSY", 0, 0 }
};
static const BitField kBits_SR2_Renesas[] = {
    { "SUS1", 7, 7 },
    { "CMP",  6, 6 },
    { "LB",   5, 3 },
    { "SUS2", 2, 2 },
    { "QE",   1, 1 },
    { "SRP1", 0, 0 }
};
static const BitField kBits_SR3_Renesas[] = {
    { "RES2", 7, 7 },
    { "DRV",  6, 5 },
    { "RES1", 4, 0 }
};
static const RegisterData kSR1_Renesas = { "SR1", 8, kBits_SR1_Renesas, 4 };
static const RegisterData kSR2_Renesas = { "SR2", 8, kBits_SR2_Renesas, 6 };
static const RegisterData kSR3_Renesas = { "SR3", 8, kBits_SR3_Renesas, 3 };

static const SpiCmdData kCmds_Renesas[] = {
    { 0x15, CM_14, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR3", "Read SR3", nullptr, nullptr },                             { &kSR3_Renesas, nullptr, nullptr, nullptr } },
    { 0x92, CM_1,  OP_DATA_READ,  kAddrGlobal, 2, 0, 0, 1, true,  false, false, { "MFID", "Read Manufacturer, Device ID DUAL I/O", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }       },
    { 0x94, CM_1,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0, 3, true,  false, false, { "MFID", "Read Manufacturer, Device ID QUAD I/O", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }       },
    { 0x4B, CM_1,  OP_DATA_READ,  0,           0, 0, 0, 4, true,  false, false, { "UID", "Read Unique ID number", nullptr, nullptr },                  { nullptr, nullptr, nullptr, nullptr }       },
    { 0x48, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "RDSECR", "Read Security Registers", nullptr, nullptr },             { nullptr, nullptr, nullptr, nullptr }       },
    { 0x01, CM_14, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR1", "Write SR1", nullptr, nullptr },                            { &kSR1_Renesas, nullptr, nullptr, nullptr } },
    { 0x31, CM_14, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR2", "Write SR2", nullptr, nullptr },                            { &kSR2_Renesas, nullptr, nullptr, nullptr } },
    { 0x11, CM_14, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR3", "Write SR3", nullptr, nullptr },                            { &kSR3_Renesas, nullptr, nullptr, nullptr } },
    { 0x50, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WRENVSR", "Write Enable for Volatile SR", nullptr, nullptr },       { nullptr, nullptr, nullptr, nullptr }       },
    { 0x77, CM_1,  OP_DATA_WRITE, 0,           4, 0, 0, 3, true,  false, false, { "SBW", "Set Burst with Wrap", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }       },
    { 0x42, CM_1,  OP_DATA_WRITE, kAddrGlobal, 0, 0, 0, 0, false, false, false, { "PRSECR", "Program Security Registers", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }       },
    { 0xF2, CM_14, OP_DATA_WRITE, kAddrGlobal, 0, 0, 0, 0, false, false, false, { "PP", "Page Program", nullptr, nullptr },                            { nullptr, nullptr, nullptr, nullptr }       },
    { 0x32, CM_1,  OP_DATA_WRITE, kAddrGlobal, 0, 4, 0, 0, false, false, false, { "QPP", "Quad Input Page Program", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr }       },
    { 0x44, CM_1,  OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "ERSECR", "Erase Security Registers", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }       },
};

// ===========================================================================
// WINBOND (id = 0xEF, parent = 0)
// ===========================================================================

static const BitField kBits_SR1_Winbond[] = {
    { "SRP0", 7, 7 },
    { "TPB",  6, 6 },
    { "TP",   5, 5 },
    { "BPB",  4, 2 },
    { "WEL",  1, 1 },
    { "BUSY", 0, 0 }
};
static const BitField kBits_SR2_Winbond[] = {
    { "SUS",  7, 7 },
    { "CMP",  6, 6 },
    { "LB",   5, 3 },
    { "QE",   1, 1 },
    { "SRP1", 0, 0 }
};
static const BitField kBits_SR3_Winbond[] = {
    { "HOLD/RESET", 7, 7 },
    { "DRV",        6, 5 },
    { "WPS",        2, 2 }
};
static const RegisterData kSR1_Winbond = { "SR1", 8, kBits_SR1_Winbond, 6 };
static const RegisterData kSR2_Winbond = { "SR2", 8, kBits_SR2_Winbond, 5 };
static const RegisterData kSR3_Winbond = { "SR3", 8, kBits_SR3_Winbond, 3 };

static const SpiCmdData kCmds_Winbond[] = {
    { 0x50, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "WRENVSR", "Write Enable for Volatile SR", nullptr, nullptr },       { nullptr, nullptr, nullptr, nullptr }             },
    { 0x35, CM_14, OP_REG_READ,   0,           0, 0, 0,    0, false, false, false, { "RDSR2", "Read SR2", nullptr, nullptr },                             { &kSR2_Winbond, nullptr, nullptr, nullptr }       },
    { 0x15, CM_14, OP_REG_READ,   0,           0, 0, 0,    0, false, false, false, { "RDSR3", "Read SR3", nullptr, nullptr },                             { &kSR3_Winbond, nullptr, nullptr, nullptr }       },
    { 0x01, CM_14, OP_REG_WRITE,  0,           0, 0, 0,    0, false, false, false, { "WRSR", "Write SR", nullptr, nullptr },                              { &kSR1_Winbond, &kSR2_Winbond, nullptr, nullptr } },
    { 0x31, CM_14, OP_REG_WRITE,  0,           0, 0, 0,    0, false, false, false, { "WRSR2", "Write SR2", nullptr, nullptr },                            { &kSR2_Winbond, nullptr, nullptr, nullptr }       },
    { 0x11, CM_14, OP_REG_WRITE,  0,           0, 0, 0,    0, false, false, false, { "WRSR3", "Write SR3", nullptr, nullptr },                            { &kSR3_Winbond, nullptr, nullptr, nullptr }       },
    { 0xEB, CM_4,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    0, false, false, true,  { "QRIO", "Fast Read Quad I/O", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }             },
    { 0xE7, CM_14, OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    1, true,  false, true,  { "W4RD", "Word Read Quad I/O", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }             },
    { 0xE3, CM_14, OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    0, false, false, true,  { "O8RD", "Octal Word Read Quad I/O", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }             },
    { 0x36, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "IBL",  "Individual Block/Sector Lock", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }             },
    { 0x39, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "IBUL", "Individual Block/Sector Unlock", nullptr, nullptr },        { nullptr, nullptr, nullptr, nullptr }             },
    { 0x3D, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "RDBL", "Read Block/Sector Lock", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr }             },
    { 0x7E, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "GBL",  "Global Block/Sector Lock", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }             },
    { 0x98, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "GBUL", "Global Block/Sector Unlock", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }             },
    { 0x77, CM_1,  OP_DATA_WRITE, 0,           4, 0, 0,    3, true,  false, false, { "SBW", "Set Burst with Wrap", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }             },
    { 0x32, CM_1,  OP_DATA_WRITE, kAddrGlobal, 0, 4, 0,    0, false, false, false, { "QPP", "Quad Input Page Program", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr }             },
    { 0x92, CM_1,  OP_DATA_READ,  kAddrGlobal, 2, 0, 0,    1, true,  false, false, { "MFID", "Read Manufacturer, Device ID DUAL I/O", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }             },
    { 0x94, CM_1,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    3, true,  false, false, { "MFID", "Read Manufacturer, Device ID QUAD I/O", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }             },
    { 0x4B, CM_1,  OP_DATA_READ,  0,           0, 0, 0,    4, true,  false, false, { "UID", "Read Unique ID number", nullptr, nullptr },                  { nullptr, nullptr, nullptr, nullptr }             },
    { 0x44, CM_1,  OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "ERSECR", "Erase Security Registers", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }             },
    { 0x42, CM_1,  OP_DATA_WRITE, kAddrGlobal, 0, 0, 0,    0, false, false, false, { "PRSECR", "Program Security Registers", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }             },
    { 0x48, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0,    1, true,  false, false, { "RDSECR", "Read Security Registers", nullptr, nullptr },             { nullptr, nullptr, nullptr, nullptr }             },
    { 0x38, CM_1,  OP_NO_DATA,    0,           0, 0, CM_4, 0, false, false, false, { "QPIEN", "Enter QPI Mode", nullptr, nullptr },                       { nullptr, nullptr, nullptr, nullptr }             },
    // QPI-only commands
    { 0x0B, CM_4,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0,    1, true,  false, false, { "FRD",  "Fast Read", nullptr, nullptr },                             { nullptr, nullptr, nullptr, nullptr }             },
    { 0xC0, CM_4,  OP_DATA_WRITE, 0,           0, 0, 0,    0, false, false, false, { "SRP",  "Set Read Parameters", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }             },
    { 0x0C, CM_4,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0,    1, true,  false, true,  { "BRW",  "Burst Read with Wrap", nullptr, nullptr },                  { nullptr, nullptr, nullptr, nullptr }             },
    { 0xFF, CM_14, OP_NO_DATA,    0,           0, 0, CM_1, 0, false, false, false, { "QPIDI", "Exit QPI Mode", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }             },
};

// ===========================================================================
// MACRONIX (id = 0xC2, parent = 0)
// ===========================================================================

static const BitField kBits_SR1_Macronix[] = {
    { "SRWD", 7, 7 },
    { "QE",   6, 6 },
    { "BPB",  5, 2 },
    { "WEL",  1, 1 },
    { "WIP",  0, 0 }
};
static const BitField kBits_CR1_Macronix[] = {
    { "DC", 6, 6 },
    { "TB", 3, 3 }
};
static const BitField kBits_CR2_Macronix[] = {
    { "L/H", 1, 1 }
};
static const BitField kBits_SecReg_Macronix[] = {
    { "E_FAIL", 6, 6 },
    { "P_FAIL", 5, 5 },
    { "ESB",    3, 3 },
    { "PSB",    2, 2 },
    { "LDSO",   1, 1 },
    { "SOTP",   0, 0 }
};
static const RegisterData kSR1_Macronix    = { "SR1", 8, kBits_SR1_Macronix, 5 };
static const RegisterData kCR1_Macronix    = { "CR1", 8, kBits_CR1_Macronix, 2 };
static const RegisterData kCR2_Macronix    = { "CR2", 8, kBits_CR2_Macronix, 1 };
static const RegisterData kSecReg_Macronix = { "Security Register", 8, kBits_SecReg_Macronix, 6 };

static const SpiCmdData kCmds_Macronix[] = {
    { 0x01, CM_1, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR", "Write SR", nullptr, nullptr },                  { &kSR1_Macronix, &kCR1_Macronix, &kCR2_Macronix, nullptr } },
    { 0x05, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR", "Read SR1", nullptr, nullptr },                  { &kSR1_Macronix, nullptr, nullptr, nullptr }               },
    { 0x15, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDCR", "Read CR", nullptr, nullptr },                   { &kCR1_Macronix, &kCR2_Macronix, nullptr, nullptr }        },
    { 0xB0, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "SUSP", "Erase/Program Suspend", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }                      },
    { 0x30, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RESM", "Erase/Program Resume", nullptr, nullptr },      { nullptr, nullptr, nullptr, nullptr }                      },
    { 0xC0, CM_1, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "SBL", "Set Burst Length", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }                      },
    { 0xB1, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "ENSO", "Enter Secured OTP", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }                      },
    { 0xC1, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "EXSO", "Exit Secured OTP", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }                      },
    { 0x2B, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSCUR", "Read Security Register", nullptr, nullptr },  { &kSecReg_Macronix, nullptr, nullptr, nullptr }            },
    { 0x2F, CM_1, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSCUR", "Write Security Register", nullptr, nullptr }, { &kSecReg_Macronix, nullptr, nullptr, nullptr }            },
    { 0xAB, CM_1, OP_DATA_READ,  0,           0, 0, 0, 3, true,  false, false, { "RES", "Read Electronic ID", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }                      },
    { 0x32, CM_1, OP_DATA_WRITE, kAddrGlobal, 0, 4, 0, 0, false, false, false, { "QPP", "Quad Input Page Program", nullptr, nullptr },    { nullptr, nullptr, nullptr, nullptr }                      },
    { 0x38, CM_1, OP_DATA_WRITE, kAddrGlobal, 4, 0, 0, 0, false, false, false, { "4PP", "Quad I/O Page Program", nullptr, nullptr },      { nullptr, nullptr, nullptr, nullptr }                      },
};

// ===========================================================================
// GIGADEVICE (id = 0xC8, parent = 0xEF)
// No commands of its own; all commands are inherited from Winbond.
// ===========================================================================

// (no GigaDevice-specific commands)

// ===========================================================================
// ADESTO (id = 0xA1, parent = 0)
// ===========================================================================

static const SpiCmdData kCmds_Adesto[] = {
    { 0xB1, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "ENSO", "Enter Secured OTP", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr } },
    { 0xC1, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "EXSO", "Exit Secured OTP", nullptr, nullptr },                      { nullptr, nullptr, nullptr, nullptr } },
    { 0x2B, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "RDSCUR", "Read Security Register", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr } },
    { 0x2F, CM_14, OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "WRSCUR", "Write Security Register", nullptr, nullptr },             { nullptr, nullptr, nullptr, nullptr } },
    { 0x38, CM_1,  OP_NO_DATA,    0,           0, 0, CM_4, 0, false, false, false, { "QPIEN", "Enter QPI Mode", nullptr, nullptr },                       { nullptr, nullptr, nullptr, nullptr } },
    { 0xFF, CM_4,  OP_NO_DATA,    0,           0, 0, CM_1, 0, false, false, false, { "QPIDI", "Exit QPI Mode", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr } },
    { 0x0C, CM_4,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0,    1, true,  false, true,  { "BRW", "Burst Read with Wrap", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr } },
    { 0xC0, CM_4,  OP_DATA_WRITE, 0,           0, 0, 0,    0, false, false, false, { "SRP", "Set Read Parameters", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr } },
    { 0x33, CM_14, OP_DATA_WRITE, kAddrGlobal, 0, 4, 0,    0, false, false, false, { "QPP", "Quad Input Page Program", nullptr, nullptr },                { nullptr, nullptr, nullptr, nullptr } },
    { 0x94, CM_1,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    3, true,  false, false, { "MFID", "Read Manufacturer, Device ID QUAD I/O", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr } },
    { 0xE7, CM_14, OP_DATA_READ,  kAddrGlobal, 4, 0, 0,    1, true,  false, true,  { "W4RD", "Word Read Quad I/O", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr } },
    { 0x77, CM_1,  OP_DATA_WRITE, 0,           4, 0, 0,    3, true,  false, false, { "SBW",  "Set Burst with Wrap", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr } },
};

// ===========================================================================
// CYPRESS (id = 0x01, parent = 0)
// ===========================================================================

static const BitField kBits_SR1_Cypress[] = {
    { "SRP0", 7, 7 },
    { "TPB",  6, 6 },
    { "TP",   5, 5 },
    { "BPB",  4, 2 },
    { "WEL",  1, 1 },
    { "BUSY", 0, 0 }
};
static const BitField kBits_SR2_Cypress[] = {
    { "SUS",  7, 7 },
    { "CMP",  6, 6 },
    { "LB",   5, 3 },
    { "QE",   1, 1 },
    { "SRP1", 0, 0 }
};
static const BitField kBits_SR3_Cypress[] = {
    { "RFU",  7, 7 },
    { "W6:5", 6, 5 },
    { "W4",   4, 4 },
    { "LC",   3, 0 }
};
static const RegisterData kSR1_Cypress = { "SR1", 8, kBits_SR1_Cypress, 6 };
static const RegisterData kSR2_Cypress = { "SR2", 8, kBits_SR2_Cypress, 5 };
static const RegisterData kSR3_Cypress = { "SR3", 8, kBits_SR3_Cypress, 4 };

static const SpiCmdData kCmds_Cypress[] = {
    { 0x05, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR", "Read SR1", nullptr, nullptr },                        { &kSR1_Cypress, nullptr, nullptr, nullptr }             },
    { 0x50, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WRENVSR", "Write Enable for Volatile SR", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x01, CM_1, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR", "Write SRs", nullptr, nullptr },                       { &kSR1_Cypress, &kSR2_Cypress, &kSR3_Cypress, nullptr } },
    { 0x07, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR2", "Read SR2", nullptr, nullptr },                       { &kSR2_Cypress, nullptr, nullptr, nullptr }             },
    { 0x35, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDCR", "Read CR", nullptr, nullptr },                         { &kSR2_Cypress, nullptr, nullptr, nullptr }             },
    { 0x33, CM_1, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR3", "Read SR3", nullptr, nullptr },                       { &kSR3_Cypress, nullptr, nullptr, nullptr }             },
    { 0xB9, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "BRAC", "Bank Register Access", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x17, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "BRWR", "Bank Register Write", nullptr, nullptr },             { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x18, CM_1, OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "ECCRD", "ECC SR Read", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x14, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "ABRD", "Auto Boot Register Read", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x43, CM_1, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "PNVDLR", "Program NVDLR", nullptr, nullptr },                 { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x4A, CM_1, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "WVDLR", "Write VDLR", nullptr, nullptr },                     { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x41, CM_1, OP_DATA_READ,  0,           0, 0, 0, 0, false, false, false, { "DLPRD", "Data Learning Pattern Read", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x30, CM_1, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "CLSR", "Clear SR", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x77, CM_1, OP_DATA_WRITE, 0,           4, 0, 0, 3, true,  false, false, { "SBW", "Set Burst with Wrap", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x39, CM_1, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "SBP", "Set Block/Pointer Protection", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x48, CM_1, OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "RDSECR", "Read Security Registers", nullptr, nullptr },       { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x44, CM_1, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "ERSECR", "Erase Security Registers", nullptr, nullptr },      { nullptr, nullptr, nullptr, nullptr }                   },
    { 0x42, CM_1, OP_DATA_WRITE, kAddrGlobal, 0, 0, 0, 0, false, false, false, { "PRSECR", "Program Security Registers", nullptr, nullptr },    { nullptr, nullptr, nullptr, nullptr }                   },
};

// ===========================================================================
// ISSI (id = 0x9D, parent = 0xEF)
// ===========================================================================

static const BitField kBits_FuncReg_Issi[] = {
    { "IRL3", 7, 7 },
    { "IRL2", 6, 6 },
    { "IRL1", 5, 5 },
    { "IRL0", 4, 2 },
    { "ESUS", 3, 3 },
    { "PSUS", 2, 2 }
};
static const RegisterData kFuncReg_Issi = { "Function Register", 8, kBits_FuncReg_Issi, 6 };

static const SpiCmdData kCmds_Issi[] = {
    { 0x48, CM_1,  OP_REG_READ,   0,           0, 0, 0,    0, false, false, false, { "RDFR", "Read Function Register", nullptr, nullptr },   { &kFuncReg_Issi, nullptr, nullptr, nullptr } },
    { 0x42, CM_1,  OP_REG_WRITE,  0,           0, 0, 0,    0, false, false, false, { "WRFR", "Write Function Register", nullptr, nullptr },  { &kFuncReg_Issi, nullptr, nullptr, nullptr } },
    { 0x68, CM_14, OP_DATA_READ,  kAddrGlobal, 0, 0, 0,    1, true,  false, false, { "IRRD", "Read Information Row", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }        },
    { 0x62, CM_14, OP_DATA_WRITE, kAddrGlobal, 0, 0, 0,    0, false, false, false, { "IRP", "Information Row Program", nullptr, nullptr },   { nullptr, nullptr, nullptr, nullptr }        },
    { 0x64, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "IRER", "Erase Information Row", nullptr, nullptr },    { nullptr, nullptr, nullptr, nullptr }        },
    { 0x26, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "SECUNLock", "Sector Unlock", nullptr, nullptr },       { nullptr, nullptr, nullptr, nullptr }        },
    { 0x24, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "SECLock", "Sector Lock", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }        },
    { 0xD7, CM_14, OP_NO_DATA,    kAddrGlobal, 0, 0, 0,    0, false, false, false, { "SE", "SER", "Sector Erase", nullptr },                 { nullptr, nullptr, nullptr, nullptr }        },
    { 0x38, CM_1,  OP_DATA_WRITE, kAddrGlobal, 0, 4, 0,    0, false, false, false, { "QPP", "Quad Input Page Program", nullptr, nullptr },   { nullptr, nullptr, nullptr, nullptr }        },
    { 0xB0, CM_1,  OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "SUSP", "Erase/Program Suspend", nullptr, nullptr },    { nullptr, nullptr, nullptr, nullptr }        },
    { 0x30, CM_1,  OP_NO_DATA,    0,           0, 0, 0,    0, false, false, false, { "RESM", "Erase/Program Resume", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }        },
    { 0x35, CM_1,  OP_NO_DATA,    0,           0, 0, CM_4, 0, false, false, false, { "QPIEN", "Enter QPI Mode", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }        },
    { 0xF5, CM_4,  OP_NO_DATA,    0,           0, 0, CM_1, 0, false, false, false, { "QPIDI", "Exit QPI Mode", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }        },
};

// ===========================================================================
// MICRON (id = 0x20, parent = 0)
// ===========================================================================

static const BitField kBits_NVCR_Micron[] = {
    { "DCC",        15, 12 },
    { "XIPMODE",    11, 9  },
    { "ODS",        8,  6  },
    { "Reset/Hold", 4,  4  },
    { "QUAD",       3,  3  },
    { "DUAL",       2,  2  }
};
static const BitField kBits_VCR_Micron[] = {
    { "DCC",  7, 4 },
    { "XIP",  3, 3 },
    { "Wrap", 1, 0 }
};
static const BitField kBits_EVCR_Micron[] = {
    { "QUAD",       7, 7 },
    { "DUAL",       6, 6 },
    { "Reset/Hold", 4, 4 },
    { "VPPACC",     3, 3 },
    { "ODS",        2, 0 }
};
static const BitField kBits_FlagSR_Micron[] = {
    { "RDY",             7, 7 },
    { "Erase suspend",   6, 6 },
    { "Erase fail",      5, 5 },
    { "Program fail",    4, 4 },
    { "VPP fail",        3, 3 },
    { "Program suspend", 2, 2 },
    { "Protection fail", 1, 1 }
};
static const BitField kBits_LockReg_Micron[] = {
    { "SLD", 1, 1 },
    { "SWL", 0, 0 }
};
static const RegisterData kNVCR_Micron    = { "Non-Volatile CR", 16, kBits_NVCR_Micron, 6 };
static const RegisterData kVCR_Micron     = { "Volatile CR", 8, kBits_VCR_Micron, 3 };
static const RegisterData kEVCR_Micron    = { "Enhanced Volatile CR", 8, kBits_EVCR_Micron, 5 };
static const RegisterData kFlagSR_Micron  = { "Flag SR", 8, kBits_FlagSR_Micron, 7 };
static const RegisterData kLockReg_Micron = { "Lock Register", 8, kBits_LockReg_Micron, 2 };

static const SpiCmdData kCmds_Micron[] = {
    { 0xAF, CM_24,  OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "MRID", "Multiple I/O Read ID", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }          },
    { 0x5A, CM_124, OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "SFDP", "Read SFDP Register", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }          },
    { 0x0B, CM_124, OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 1, true,  false, false, { "FRD", "Fast Read", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }          },
    { 0x3B, CM_12,  OP_DATA_READ,  kAddrGlobal, 0, 2, 0, 1, true,  false, false, { "DRD",  "Fast Read Dual Output", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }          },
    { 0xBB, CM_12,  OP_DATA_READ,  kAddrGlobal, 2, 0, 0, 1, true,  false, true,  { "DRIO", "Fast Read Dual I/O", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }          },
    { 0x6B, CM_14,  OP_DATA_READ,  kAddrGlobal, 0, 4, 0, 1, true,  false, false, { "QRD",  "Fast Read Quad Output", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }          },
    { 0xEB, CM_14,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0, 2, true,  false, true,  { "QRIO", "Fast Read Quad I/O", nullptr, nullptr },              { nullptr, nullptr, nullptr, nullptr }          },
    { 0x06, CM_124, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WREN", "Write Enable", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }          },
    { 0x04, CM_124, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "WRDI", "Write Disable", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }          },
    { 0x05, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR", "Read SR", nullptr, nullptr },                         { nullptr, nullptr, nullptr, nullptr }          },
    { 0x01, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRSR", "Write SR", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }          },
    { 0xE8, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDLR", "Read Lock Register", nullptr, nullptr },              { &kLockReg_Micron, nullptr, nullptr, nullptr } },
    { 0xE5, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRLR", "Write Lock Register", nullptr, nullptr },             { &kLockReg_Micron, nullptr, nullptr, nullptr } },
    { 0x70, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDFSR", "Read Flag SR", nullptr, nullptr },                   { &kFlagSR_Micron, nullptr, nullptr, nullptr }  },
    { 0x50, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRFSR", "Write Flag SR", nullptr, nullptr },                  { &kFlagSR_Micron, nullptr, nullptr, nullptr }  },
    { 0xB5, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDNVCR", "Read Non-Volatile CR", nullptr, nullptr },          { &kNVCR_Micron, nullptr, nullptr, nullptr }    },
    { 0xB1, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRNVCR", "Write Non-Volatile CR", nullptr, nullptr },         { &kNVCR_Micron, nullptr, nullptr, nullptr }    },
    { 0x85, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDVCR", "Read Volatile CR", nullptr, nullptr },               { &kVCR_Micron, nullptr, nullptr, nullptr }     },
    { 0x81, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WRVCR", "Write Volatile CR", nullptr, nullptr },              { &kVCR_Micron, nullptr, nullptr, nullptr }     },
    { 0x85, CM_124, OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDEVCR", "Read Enhanced Volatile CR", nullptr, nullptr },     { &kEVCR_Micron, nullptr, nullptr, nullptr }    },
    { 0x81, CM_124, OP_REG_WRITE,  0,           0, 0, 0, 0, false, false, false, { "WREVCR", "Write Enhanced Volatile CR", nullptr, nullptr },    { &kEVCR_Micron, nullptr, nullptr, nullptr }    },
    { 0x02, CM_124, OP_DATA_WRITE, kAddrGlobal, 0, 0, 0, 0, false, false, false, { "PP", "Page Program", nullptr, nullptr },                      { nullptr, nullptr, nullptr, nullptr }          },
    { 0xA2, CM_12,  OP_DATA_WRITE, kAddrGlobal, 0, 2, 0, 0, false, false, false, { "DPP", "Dual Input Fast Program", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }          },
    { 0xD2, CM_12,  OP_DATA_WRITE, kAddrGlobal, 2, 0, 0, 0, false, false, false, { "DPP", "Extended Dual Input Fast Program", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }          },
    { 0x32, CM_14,  OP_DATA_WRITE, kAddrGlobal, 0, 2, 0, 0, false, false, false, { "QPP", "Quad Input Fast program", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }          },
    { 0x12, CM_14,  OP_DATA_WRITE, kAddrGlobal, 2, 0, 0, 0, false, false, false, { "QPP", "Extended Quad Input Fast Program", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }          },
    { 0x20, CM_124, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "SSE", "Subsector Erase", nullptr, nullptr },                  { nullptr, nullptr, nullptr, nullptr }          },
    { 0xD8, CM_124, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "SE", "Sector Erase", nullptr, nullptr },                      { nullptr, nullptr, nullptr, nullptr }          },
    { 0xC7, CM_124, OP_NO_DATA,    kAddrGlobal, 0, 0, 0, 0, false, false, false, { "BE", "Bulk Erase", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }          },
    { 0x75, CM_124, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "SUSP", "Erase/Program Suspend", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }          },
    { 0x7A, CM_124, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RESM", "Erase/Program Resume", nullptr, nullptr },            { nullptr, nullptr, nullptr, nullptr }          },
    { 0x75, CM_124, OP_DATA_READ,  0,           0, 0, 0, 0, false, false, false, { "ROTP", "Read OTP Array", nullptr, nullptr },                  { nullptr, nullptr, nullptr, nullptr }          },
    { 0x7A, CM_124, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "POTP", "Program OTP Array", nullptr, nullptr },               { nullptr, nullptr, nullptr, nullptr }          },
};

// ===========================================================================
// MICROCHIP (id = 0xBF, parent = 0xEF)
// ===========================================================================

static const BitField kBits_SR_Microchip[] = {
    { "BUSY", 7, 7 },
    { "SEC",  5, 5 },
    { "WPLD", 4, 4 },
    { "WSP",  3, 3 },
    { "WSE",  2, 2 },
    { "WEL",  1, 1 },
    { "BUSY", 0, 0 }
};
static const BitField kBits_CR_Microchip[] = {
    { "WPEN", 7, 7 },
    { "BPNV", 3, 3 },
    { "IOC",  1, 1 }
};
static const RegisterData kSR_Microchip = { "SR", 8, kBits_SR_Microchip, 7 };
static const RegisterData kCR_Microchip = { "CR", 8, kBits_CR_Microchip, 3 };

static const SpiCmdData kCmds_Microchip[] = {
    { 0x05, CM_1,  OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDSR", "Read SR", nullptr, nullptr },                                 { &kSR_Microchip, nullptr, nullptr, nullptr } },
    { 0x05, CM_4,  OP_REG_READ,   0,           0, 0, 0, 1, true,  false, false, { "RDSR", "Read SR", nullptr, nullptr },                                 { &kSR_Microchip, nullptr, nullptr, nullptr } },
    { 0x35, CM_1,  OP_REG_READ,   0,           0, 0, 0, 0, false, false, false, { "RDCR", "Read CR", nullptr, nullptr },                                 { &kCR_Microchip, nullptr, nullptr, nullptr } },
    { 0x35, CM_4,  OP_REG_READ,   0,           0, 0, 0, 1, true,  false, false, { "RDCR", "Read CR", nullptr, nullptr },                                 { &kCR_Microchip, nullptr, nullptr, nullptr } },
    { 0x0B, CM_4,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 3, true,  false, false, { "FRD", "Fast Read", nullptr, nullptr },                                { nullptr, nullptr, nullptr, nullptr }        },
    { 0xEB, CM_1,  OP_DATA_READ,  kAddrGlobal, 4, 0, 0, 3, true,  false, true,  { "QRIO", "Fast Read Quad I/O", nullptr, nullptr },                      { nullptr, nullptr, nullptr, nullptr }        },
    { 0xC0, CM_14, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "SB", "Set Burst Length", nullptr, nullptr },                          { nullptr, nullptr, nullptr, nullptr }        },
    { 0x0C, CM_4,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 3, true,  false, true,  { "RBSQI", "Burst Read with Wrap", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }        },
    { 0xEC, CM_1,  OP_DATA_READ,  kAddrGlobal, 0, 0, 0, 3, true,  false, true,  { "RBSPI", "Burst Read with Wrap", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }        },
    { 0xB0, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "SUSP", "Erase/Program Suspend", nullptr, nullptr },                   { nullptr, nullptr, nullptr, nullptr }        },
    { 0x30, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "RESM", "Erase/Program Resume", nullptr, nullptr },                    { nullptr, nullptr, nullptr, nullptr }        },
    { 0x72, CM_1,  OP_DATA_READ,  0,           0, 0, 0, 0, false, false, false, { "RBPR", "Read Block Protection Register", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }        },
    { 0x72, CM_4,  OP_DATA_READ,  0,           0, 0, 0, 1, true,  false, false, { "RBPR", "Read Block Protection Register", nullptr, nullptr },          { nullptr, nullptr, nullptr, nullptr }        },
    { 0x8D, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "LBPR", "Lock Down Block Protection Register", nullptr, nullptr },     { nullptr, nullptr, nullptr, nullptr }        },
    { 0xE8, CM_14, OP_DATA_WRITE, 0,           0, 0, 0, 0, false, false, false, { "nVWLDR", "Non-Volatile Write Lock Down Register", nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr }        },
    { 0x98, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "ULBPR", "Global Block Protection Unlock", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }        },
    { 0x88, CM_1,  OP_DATA_READ,  kAddr16,     0, 0, 0, 1, true,  false, false, { "RSID", "Read Security ID", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }        },
    { 0x88, CM_4,  OP_DATA_READ,  kAddr16,     0, 0, 0, 3, true,  false, false, { "RSID", "Read Security ID", nullptr, nullptr },                        { nullptr, nullptr, nullptr, nullptr }        },
    { 0xA5, CM_14, OP_DATA_WRITE, kAddr16,     0, 0, 0, 0, false, false, false, { "PSID", "Program User Security ID Area", nullptr, nullptr },           { nullptr, nullptr, nullptr, nullptr }        },
    { 0x85, CM_14, OP_NO_DATA,    0,           0, 0, 0, 0, false, false, false, { "LSID", "Lockout Security ID Programming", nullptr, nullptr },         { nullptr, nullptr, nullptr, nullptr }        },
};

// ===========================================================================
// FUJITSU FRAM (id = 0x04, parent = 0)
// No device-specific commands; all commands are inherited from Common.
// ===========================================================================

// (no Fujitsu-specific commands)

// ===========================================================================
// Master command-set table
// ===========================================================================

static const CmdSet kCmdSets[] = {
    // { id, name,     parentId, cmds,            cmdCount }
    { 0x00, "Unspecified",   -1, kCmds_Common,    static_cast<uint16_t>(std::size(kCmds_Common))    },
    { 0xA1, "Adesto",      0x00, kCmds_Adesto,    static_cast<uint16_t>(std::size(kCmds_Adesto))    },
    { 0x01, "Cypress",     0x00, kCmds_Cypress,   static_cast<uint16_t>(std::size(kCmds_Cypress))   },
    { 0x04, "Fujitsu",     0x00, nullptr,         0                                                 },
    { 0xC8, "GigaDevice",  0xEF, nullptr,         0                                                 },
    { 0x9D, "Issi",        0xEF, kCmds_Issi,      static_cast<uint16_t>(std::size(kCmds_Issi))      },
    { 0xC2, "Macronix",    0x00, kCmds_Macronix,  static_cast<uint16_t>(std::size(kCmds_Macronix))  },
    { 0xBF, "Microchip",   0xEF, kCmds_Microchip, static_cast<uint16_t>(std::size(kCmds_Microchip)) },
    { 0x20, "Micron",      0x00, kCmds_Micron,    static_cast<uint16_t>(std::size(kCmds_Micron))    },
    { 0x1F, "Renesas",     0x00, kCmds_Renesas,   static_cast<uint16_t>(std::size(kCmds_Renesas))   },
    { 0xEF, "Winbond",     0x00, kCmds_Winbond,   static_cast<uint16_t>(std::size(kCmds_Winbond))   },
};
static constexpr size_t kCmdSetCount = std::size(kCmdSets);

// ---------------------------------------------------------------------------
// Internal helper: look up a CmdSet entry by id (returns first match).
// ---------------------------------------------------------------------------
static const CmdSet* FindCmdSetById(int id)
{
    if (id < 0)
    {
        return nullptr;
    }
    for (size_t i = 0; i < kCmdSetCount; ++i)
    {
        if (kCmdSets[i].mId == id)
        {
            return &kCmdSets[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// CmdSet method implementations
// ---------------------------------------------------------------------------

const SpiCmdData* CmdSet::FindCommand(BusMode mode, uint8_t code) const
{
    for (uint16_t i = 0; i < mCmdCount; ++i)
    {
        if (mCmds[i].mCode == code && mCmds[i].IsValidForMode(mode))
        {
            return &mCmds[i];
        }
    }

    return nullptr;
}

const SpiCmdData* CmdSet::GetCommand(BusMode mode, uint8_t code) const
{
    const SpiCmdData* cmd = FindCommand(mode, code);
    if (cmd)
    {
        return cmd;
    }

    const CmdSet* parent = FindCmdSetById(mParentId);
    return parent ? parent->GetCommand(mode, code) : nullptr;
}

void CmdSet::GetValidCommands(BusMode mode, std::vector<uint8_t>& cmds) const
{
    // Collect from parent chain first, then add child entries not already present
    const CmdSet* parent = FindCmdSetById(mParentId);
    if (parent)
    {
        parent->GetValidCommands(mode, cmds);
    }

    for (uint16_t i = 0; i < mCmdCount; ++i)
    {
        if (!mCmds[i].IsValidForMode(mode))
        {
            continue;
        }

        uint8_t c  = mCmds[i].mCode;
        bool found = false;

        for (auto x : cmds)
        {
            if (x == c)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            cmds.push_back(c);
        }
    }

    if (parent)
    {
        std::sort(cmds.begin(), cmds.end());
        cmds.erase(std::unique(cmds.begin(), cmds.end()), cmds.end());
    }
}

void CmdSet::GetContinuousReadCommands(std::vector<const SpiCmdData*>& cmds) const
{
    const CmdSet* parent = FindCmdSetById(mParentId);
    if (parent)
    {
        parent->GetContinuousReadCommands(cmds);
    }

    for (uint16_t i = 0; i < mCmdCount; ++i)
    {
        if (!mCmds[i].mContinuousRead)
        {
            continue;
        }

        bool found = false;
        for (const SpiCmdData* existing : cmds)
        {
            if (existing->mCode == mCmds[i].mCode)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            cmds.push_back(&mCmds[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// SpiFlash runtime class
// ---------------------------------------------------------------------------

SpiFlash::SpiFlash()
    : mActiveCmdSet(nullptr)
    , mCurrentCmd(nullptr)
    , mSpiMode(SPI_MODE0)
    , mDefBusMode(SINGLE)
    , mCurBusMode(SINGLE)
    , mAddressBits(24)
    , mDataIn(false)
{
    for (size_t i = 0; i < kCmdSetCount; ++i)
    {
        mCmdSetPtrs.push_back(&kCmdSets[i]);
    }

    SelectCmdSet(0); // default to Common ("Unspecified")
}

const CmdSet* SpiFlash::GetCommandSet(int id) const
{
    return FindCmdSetById(id);
}

void SpiFlash::SelectCmdSet(int id)
{
    mActiveCmdSet = GetCommandSet(id);
}

const SpiCmdData* SpiFlash::GetCommand(BusMode mode, uint8_t code) const
{
    return mActiveCmdSet ? mActiveCmdSet->GetCommand(mode, code) : nullptr;
}

void SpiFlash::GetValidCommands(std::vector<uint8_t>& cmds) const
{
    cmds.clear();
    if (mActiveCmdSet)
    {
        mActiveCmdSet->GetValidCommands(mCurBusMode, cmds);
    }
}

SpiFlash spiFlash;
