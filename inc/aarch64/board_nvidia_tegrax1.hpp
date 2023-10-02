/*
 * Board-Specific Configuration: NVIDIA Tegra X1 (Erista)
 *
 * Copyright (C) 2019-2023 Udo Steinberg, BedRock Systems, Inc.
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

#define RAM_BASE    0x80000000  // 0x80000000 - 0xffffffff

#ifndef __ASSEMBLER__

#include "debug.hpp"

struct Board
{
    static constexpr uint64_t spin_addr { 0 };
    static constexpr struct Cpu { uint64_t id; } cpu[4] { { 0x0 }, { 0x1 }, { 0x2 }, { 0x3 } };
    static constexpr struct Tmr { unsigned ppi, flg; } tmr[2] { { 0xa, 0x8 }, { 0xb, 0x8 } };
    static constexpr struct Gic { uint64_t mmio, size; } gic[4] { { 0x50041000, 0x1000 }, { 0, 0 }, { 0x50042000, 0x2000 }, { 0x50044000, 0x2000 } };
    static constexpr struct Uart { Debug::Subtype type; uint64_t mmio; unsigned clock; } uart { Debug::Subtype::SERIAL_NS16550_NVIDIA, 0x70006000, 408'000'000 };
    static constexpr struct Smmu { uint64_t mmio; struct { unsigned spi, flg; } glb[1], ctx[1]; } smmu[1] { { 0, { { 0, 0 } }, { { 0, 0 } } } };
};

#endif
