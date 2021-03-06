/*
 * I/O Advanced Programmable Interrupt Controller (IOAPIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019-2021 Udo Steinberg, BedRock Systems, Inc.
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

#include "extern.hpp"
#include "ioapic.hpp"
#include "pd_kern.hpp"
#include "stdio.hpp"

INIT_PRIORITY (PRIO_SLAB)
Slab_cache Ioapic::cache (sizeof (Ioapic), 8);

Ioapic::Ioapic (Paddr p, unsigned i, unsigned g) : List<Ioapic> (list), reg_base ((hwdev_addr -= PAGE_SIZE) | (p & PAGE_MASK)), gsi_base (g), id (i), rid (0)
{
    // Reserve MMIO region
    Pd_kern::remove_user_mem (p & ~PAGE_MASK, PAGE_SIZE);

    Hptp::master.update (reg_base, p & ~PAGE_MASK, 0, Paging::Permissions (Paging::G | Paging::W | Paging::R), Memattr::Cacheability::MEM_UC, Memattr::Shareability::NONE);

    trace (TRACE_INTR, "APIC: %#010lx ID:%#x VER:%#x PRQ:%u GSI:%u-%u", p, i, version(), prq(), gsi_base, gsi_base + irt_max());
}
