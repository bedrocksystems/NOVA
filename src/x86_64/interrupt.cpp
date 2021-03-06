/*
 * Interrupt Handling
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

#include "counter.hpp"
#include "hazards.hpp"
#include "interrupt.hpp"
#include "ioapic.hpp"
#include "lapic.hpp"
#include "pd_kern.hpp"
#include "sm.hpp"
#include "smmu.hpp"
#include "stdio.hpp"
#include "vectors.hpp"

Interrupt Interrupt::int_table[NUM_GSI];

void Interrupt::init()
{
    for (unsigned i = 0; i < sizeof (int_table) / sizeof (*int_table); i++)
        int_table[i].sm = Sm::create (0, i);
}

void Interrupt::set_mask (unsigned gsi, bool msk)
{
    if (int_table[gsi].rte & BIT (15)) {

        auto ioapic = int_table[gsi].ioapic;

        if (ioapic)
            ioapic->set_rte_lo (gsi, msk << 16 | int_table[gsi].rte);
    }
}

void Interrupt::rke_handler()
{
    if (Pd::current->Space_mem::htlb.chk (Cpu::id))
        Cpu::hazard |= HZD_SCHED;
}

void Interrupt::handle_ipi (unsigned vector)
{
    unsigned ipi = vector - VEC_IPI;

    Counter::req[ipi].inc();

    switch (ipi) {
        case Request::RRQ: Scheduler::requeue(); break;
        case Request::RKE: rke_handler(); break;
    }

    Lapic::eoi();
}

void Interrupt::handle_lvt (unsigned vector)
{
    unsigned lvt = vector - VEC_LVT;

    Counter::loc[lvt].inc();

    switch (vector) {
        case VEC_LVT_TIMER: Lapic::timer_handler(); break;
        case VEC_LVT_ERROR: Lapic::error_handler(); break;
        case VEC_LVT_PERFM: Lapic::perfm_handler(); break;
        case VEC_LVT_THERM: Lapic::therm_handler(); break;
    }

    Lapic::eoi();
}

void Interrupt::handle_gsi (unsigned vector)
{
    unsigned gsi = vector - VEC_GSI;

    Counter::gsi[gsi].inc();

    set_mask (gsi, true);

    Lapic::eoi();

    int_table[gsi].sm->up();
}

void Interrupt::configure (unsigned gsi, unsigned flags, unsigned cpu, uint16 rid, uint32 &msi_addr, uint16 &msi_data)
{
    bool msk = flags & BIT (0);     // 0 = unmasked, 1 = masked
    bool trg = flags & BIT (1);     // 0 = edge, 1 = level
    bool pol = flags & BIT (2);     // 0 = high, 1 = low
    auto aid = Cpu::apic_id[cpu];

    trace (TRACE_INTR, "INTR: %s: %u cpu=%u %c%c%c", __func__, gsi, cpu, msk ? 'M' : 'U', trg ? 'L' : 'E', pol ? 'L' : 'H');

    int_table[gsi].cpu = static_cast<uint16>(cpu);
    int_table[gsi].rte = static_cast<uint16>(trg << 15 | pol << 13 | (VEC_GSI + gsi));

    auto ioapic = int_table[gsi].ioapic;

    if (ioapic)
        rid = ioapic->get_rid();

    Smmu::set_irt (gsi, rid, aid, VEC_GSI + gsi, trg);

    /* MSI Compatibility Format
     * ADDR: 0xfee[31:20] APICID[19:12] ---[11:5] 0[4] RH[3] DM[2] --[1:0]
     * DATA: ---[31:16] TRG[15] ASS[14] --[13:11] DLVM[10:8] VEC[7:0]
     *
     * MSI Remappable Format
     * ADDR: 0xfee[31:20] Handle[19:5] 1[4] SHV[3] Handle[2] --[1:0]
     * DATA: ---[31:16] Subhandle[15:0]
     */

    /* IOAPIC RTE Format
     * 63:56/31:24 = APICID     => ADDR[19:12]
     * 55:48/23:16 = EDID       => ADDR[11:4]
     * 16 = MSK
     * 15 = TRG                 => DATA[15]
     * 14 = RIRR
     * 13 = POL
     * 12 = DS
     * 11 = DM                  => ADDR[2]
     * 10:8 = DLV               => DATA[10:8]
     * 7:0 = VEC                => DATA[7:0]
     */
    if (ioapic) {
        ioapic->set_rte_hi (gsi, Smmu::ire() ? gsi << 17 | BIT (16) : aid << 24);
        ioapic->set_rte_lo (gsi, msk << 16 | int_table[gsi].rte);
        msi_addr = msi_data = 0;
    } else {
        msi_addr = 0xfee << 20 | (Smmu::ire() ? BIT_RANGE (4, 3) : aid << 12);
        msi_data = uint16 (Smmu::ire() ? gsi : VEC_GSI + gsi);
    }
}

void Interrupt::send_cpu (unsigned cpu, Request req)
{
    Lapic::send_ipi (cpu, VEC_IPI + req);
}
