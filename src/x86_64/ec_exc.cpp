/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#include "ec.hpp"
#include "extern.hpp"
#include "gdt.hpp"
#include "mca.hpp"
#include "stdio.hpp"

void Ec::load_fpu()
{
    if (!utcb)
        regs.fpu_ctrl (true);

    if (EXPECT_FALSE (!fpu))
        Fpu::init();
    else
        fpu->load();
}

void Ec::save_fpu()
{
    if (!utcb)
        regs.fpu_ctrl (false);

    if (EXPECT_FALSE (!fpu))
        fpu = new Fpu;

    fpu->save();
}

void Ec::transfer_fpu (Ec *ec)
{
    if (!(Cpu::hazard & Hazard::FPU)) {

        Fpu::enable();

        if (fpowner != this) {
            if (fpowner)
                fpowner->save_fpu();
            load_fpu();
        }
    }

    fpowner = ec;
}

void Ec::handle_exc_nm()
{
    Fpu::enable();

    if (current == fpowner)
        return;

    if (fpowner)
        fpowner->save_fpu();

    current->load_fpu();

    fpowner = current;
}

bool Ec::handle_exc_gp (Exc_regs *)
{
    if (Cpu::hazard & Hazard::TR) {
        Cpu::hazard &= ~Hazard::TR;
        Tss::load();
        return true;
    }

    return false;
}

bool Ec::handle_exc_pf (Exc_regs *r)
{
    mword addr = r->cr2;

    if (r->err & BIT (2))       // User-mode access
        return addr < USER_ADDR && Pd::current->Space_hst::loc[Cpu::id].share_from (Pd::current->Space_hst::hptp, addr, USER_ADDR);

    if (addr >= LINK_ADDR && addr < MMAP_CPU && Pd::current->Space_hst::loc[Cpu::id].share_from_master (addr))
        return true;

    // Kernel fault in PIO space
    if (addr >= MMAP_SPC_PIO && addr <= MMAP_SPC_PIO_E && Pd::current->Space_hst::loc[Cpu::id].share_from (Pd::current->Space_hst::hptp, addr, MMAP_CPU))
        return true;

    // Convert #PF in I/O bitmap to #GP(0)
    if (r->user() && addr >= MMAP_SPC_PIO && addr <= MMAP_SPC_PIO_E) {
        r->vec = EXC_GP;
        r->err = r->cr2 = 0;
        send_msg<ret_user_iret>();
    }

    die ("#PF (kernel)", r);
}

void Ec::handle_exc (Exc_regs *r)
{
    switch (r->vec) {

        case EXC_NM:
            handle_exc_nm();
            return;

        case EXC_GP:
            if (handle_exc_gp (r))
                return;
            break;

        case EXC_PF:
            if (handle_exc_pf (r))
                return;
            break;

        case EXC_MC:
            Mca::vector();
            break;
    }

    if (r->user())
        send_msg<ret_user_iret>();

    die ("EXC", r);
}
