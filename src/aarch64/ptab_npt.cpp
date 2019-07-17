/*
 * Nested Page Table (NPT)
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

#include "ptab_npt.hpp"

uint64 Nptp::current;

void Nptp::init()
{
    // Reset at resume time to match vttbr
    current = 0;

    auto const oas { 2 };

    // IPA cannot be larger than OAS supported by CPU
    assert (Npt::ibits <= Npt::pas (oas));

    asm volatile ("msr vtcr_el2, %x0; isb" : : "rZ" (VTCR_RES1 | oas << 16 | TCR_A64_TG0_4K | TCR_ALL_SH0_INNER | TCR_ALL_ORGN0_WB_RW | TCR_ALL_IRGN0_WB_RW | (Npt::lev() - 2) << 6 | (64 - Npt::ibits)) : "memory");
}
