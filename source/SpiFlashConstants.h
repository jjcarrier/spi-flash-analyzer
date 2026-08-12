/*
 * MIT License
 *
 * Copyright(c) 2026 Jon Carrier
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

#ifndef SPI_FLASH_CONSTANTS_H
#define SPI_FLASH_CONSTANTS_H

// Address/bit sizes
constexpr int SPI_FLASH_ADDR_BITS_8  = 8;
constexpr int SPI_FLASH_ADDR_BITS_16 = 16;
constexpr int SPI_FLASH_ADDR_BITS_24 = 24;
constexpr int SPI_FLASH_ADDR_BITS_32 = 32;

constexpr unsigned SPI_FLASH_ADDR_8_MAX  = 0x100;
constexpr unsigned SPI_FLASH_ADDR_16_MAX = 0x10000;
constexpr unsigned SPI_FLASH_ADDR_24_MAX = 0x1000000;

constexpr unsigned SPI_FLASH_INVALID_CMD  = 0x100;
constexpr unsigned SPI_FLASH_CMD_NOT_SET  = 0xFF;
constexpr unsigned SPI_FLASH_ADDR_NOT_SET = 0xFF;

// Buffer sizes
constexpr int SPI_FLASH_NUMSTR_BUF_SIZE    = 128;
constexpr int SPI_FLASH_OFFSETSTR_BUF_SIZE = 64;
constexpr int SPI_FLASH_EDGES_BUF_SIZE     = 10;
constexpr int SPI_FLASH_CACHED_CLOCKS      = (2 * SPI_FLASH_ADDR_BITS_32) + 1;

// Simulation/data generation
constexpr int SPI_FLASH_DELAY_MIN   = 3;
constexpr int SPI_FLASH_DELAY_RANGE = 10;
constexpr int SPI_FLASH_DATA_MIN    = 1;
constexpr int SPI_FLASH_DATA_MAX    = 50;

// SPI protocol
constexpr int SPI_FLASH_CS_INDEX_LIMIT      = 1000;
constexpr unsigned SPI_FLASH_MAX_CLOCK_FREQ = 104000000U;
constexpr int SPI_FLASH_SIM_CLOCK_ADVANCE   = 100;

// Bit masks for GenerateByte
constexpr unsigned SPI_FLASH_MASK_NONE = 0x00;
constexpr unsigned SPI_FLASH_MASK_1B   = 0x80;
constexpr unsigned SPI_FLASH_MASK_2B   = 0xC0;
constexpr unsigned SPI_FLASH_MASK_4B   = 0xF0;

// Misc
constexpr unsigned SPI_FLASH_HALF_CLOCK_DELAY = 128;

#endif // SPI_FLASH_CONSTANTS_H
