/*
 * I/O Advanced Programmable Interrupt Controller (IOAPIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#include "ioapic.hpp"
#include "pd.hpp"
#include "stdio.hpp"

INIT_PRIORITY (PRIO_SLAB) Slab_cache Ioapic::cache { sizeof (Ioapic), alignof (Ioapic) };

Ioapic::Ioapic (uint64_t p, uint8_t i, unsigned g) : List (list), reg_base (mmap | (p & OFFS_MASK (0))), gsi_base (g), id (i)
{
    mmap += PAGE_SIZE (0);

#if 0   // FIXME
    Pd::kern.Space_mem::delreg (p & ~OFFS_MASK (0));
#endif

    Pd::kern.Space_mem::insert (reg_base, 0, Hpt::HPT_NX | Hpt::HPT_G | Hpt::HPT_UC | Hpt::HPT_W | Hpt::HPT_P, p & ~OFFS_MASK (0));
}

void Ioapic::init()
{
    trace (TRACE_INTR, "APIC: I/O:%#04x VER:%#x GSI:%#04x-%#04x (%02x:%02x.%x)", id, ver(), gsi_base, gsi_base + mre(), dev >> 8, dev >> 3 & BIT_RANGE (4, 0), dev & BIT_RANGE (2, 0));

    for (unsigned i { 0 }; i <= mre(); i++)
        set_cfg (gsi_base + i);
}
