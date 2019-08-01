/*
 * Board-Specific Configuration: Amlogic SM1 (S905D3/S905X3/S905Y3)
 *
 * Copyright (C) 2019-2022 Udo Steinberg, BedRock Systems, Inc.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#define LOAD_ADDR       0x7f000000

#ifndef __ASSEMBLER__

#include "types.hpp"

struct Board
{
    static constexpr uint64 spin_addr { 0 };
    static constexpr struct Cpu { uint64 id; } cpu[4] { { 0x0 }, { 0x1 }, { 0x2 }, { 0x3 } };
    static constexpr struct Tmr { unsigned ppi, flg; } tmr[2] { { 0xa, 0x8 }, { 0xb, 0x8 } };
    static constexpr struct Gic { uint64 mmio, size; } gic[4] { { 0xffc01000, 0x1000 }, { 0, 0 }, { 0xffc02000, 0x2000 }, { 0xffc04000, 0x2000 } };
    static constexpr struct Uart { Uart_type type; uint64 mmio; unsigned clock; } uart { Uart_type::MESON, 0xff803000, 24'000'000 };
    static constexpr struct Smmu { uint64 mmio; struct { unsigned spi, flg; } glb[1], ctx[1]; } smmu[1] { { 0, { { 0, 0 } }, { { 0, 0 } } } };
};

#endif
