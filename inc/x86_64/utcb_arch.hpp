/*
 * User Thread Control Block (UTCB): Architecture-Specific (x86)
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

#pragma once

#include "mtd_arch.hpp"

class Cpu_regs;

class Utcb_segment
{
    public:
        uint16  sel, ar;
        uint32  limit;
        uint64  base;

        ALWAYS_INLINE
        inline void set_vmx (uint16 s, uint64 b, uint32 l, uint32 a)
        {
            sel   = s;
            ar    = static_cast<uint16>((a >> 4 & 0x1f00) | (a & 0xff));
            limit = l;
            base  = b;
        }
};

class Utcb_arch
{
    private:
        uint64          rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
        uint64          r8,  r9,  r10, r11, r12, r13, r14, r15;
        uint64          rfl, rip;
        uint32          inst_len, inst_info, intr_state, actv_state;
        uint64          qual[2];
        uint32          ctrl0, ctrl1;
        uint64          ctrl2;
        uint32          pfe_mask, pfe_match, exc_bitmap, tpr_threshold;
        uint32          intr_info, intr_errc;
        uint32          vect_info, vect_errc;
        Utcb_segment    cs, ss, ds, es, fs, gs, tr, ld, gd, id;
        uint64          pdpte[4];
        uint64          cr0, cr2, cr3, cr4, cr8, dr7;
        uint64          sysenter_cs, sysenter_esp, sysenter_eip;
        uint64          pat, efer;
        uint64          star, lstar, fmask;
        uint64          kernel_gs_base;

    public:
        void load_exc (Mtd_arch, Cpu_regs const *);
        void load_vmx (Mtd_arch, Cpu_regs const *);
        void load_svm (Mtd_arch, Cpu_regs const *);
        bool save_exc (Mtd_arch, Cpu_regs *) const;
        bool save_vmx (Mtd_arch, Cpu_regs *) const;
        bool save_svm (Mtd_arch, Cpu_regs *) const;
};
