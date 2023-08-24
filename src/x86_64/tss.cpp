/*
 * Task State Segment (TSS)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "hpt.hpp"
#include "tss.hpp"

ALIGNED(8) Tss Tss::run;
ALIGNED(8) Tss Tss::dbf;

void Tss::build()
{
#ifdef __i386__
    extern char tss_handler;
    dbf.cr3     = Hpt::current();
    dbf.eip     = reinterpret_cast<mword>(&tss_handler);
    dbf.esp     = CPU_LOCAL_STCK + PAGE_SIZE;
    dbf.eflags  = 2;
    dbf.cs      = SEL_KERN_CODE;
    dbf.ds      = SEL_KERN_DATA;
    dbf.es      = SEL_KERN_DATA;
    dbf.ss      = SEL_KERN_DATA;
    run.ss0     = SEL_KERN_DATA;
#endif

    run.sp0     = CPU_LOCAL_STCK + PAGE_SIZE;
    run.iobm    = static_cast<uint16>(SPC_LOCAL_IOP - reinterpret_cast<mword>(&run));
}
