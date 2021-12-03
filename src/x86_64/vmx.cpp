/*
 * Virtual Machine Extensions (VMX)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "acpi.hpp"
#include "arch.hpp"
#include "bits.hpp"
#include "cmdline.hpp"
#include "extern.hpp"
#include "gdt.hpp"
#include "hip.hpp"
#include "idt.hpp"
#include "lowlevel.hpp"
#include "msr.hpp"
#include "ptab_ept.hpp"
#include "stdio.hpp"
#include "tss.hpp"
#include "util.hpp"
#include "vmx.hpp"

Vmcs *              Vmcs::current       { nullptr };
Vmcs *              Vmcs::root          { nullptr };
Vmcs::vmx_basic     Vmcs::basic;
Vmcs::vmx_ept_vpid  Vmcs::ept_vpid;
Vmcs::vmx_ctrl_pin  Vmcs::ctrl_pin;
Vmcs::vmx_ctrl_cpu  Vmcs::ctrl_cpu[3];
Vmcs::vmx_ctrl_exi  Vmcs::ctrl_exi;
Vmcs::vmx_ctrl_ent  Vmcs::ctrl_ent;
uintptr_t           Vmcs::fix_cr0_set, Vmcs::fix_cr0_clr;
uintptr_t           Vmcs::fix_cr4_set, Vmcs::fix_cr4_clr;

void Vmcs::init (Bitmap_pio *pio, Bitmap_msr *msr, uintptr_t rsp, uintptr_t cr3, uint64 eptp, uint16 vpid)
{
    // Set VMCS launch state to "clear" and initialize implementation-specific VMCS state.
    clear();

    make_current();

    uint32 pin = PIN_VIRT_NMI | PIN_NMI | PIN_EXTINT;
    uint32 exi = EXI_LOAD_EFER | EXI_SAVE_EFER | EXI_LOAD_PAT | EXI_SAVE_PAT | EXI_INTA | EXI_HOST_64;
    uint32 ent = ENT_LOAD_EFER | ENT_LOAD_PAT;

    write (Encoding::PF_ERROR_MASK, 0);
    write (Encoding::PF_ERROR_MATCH, 0);
    write (Encoding::CR3_TARGET_COUNT, 0);

    write (Encoding::VMCS_LINK_PTR, ~0ULL);

    write (Encoding::VPID, vpid);
    write (Encoding::EPTP, eptp | (Eptp::lev - 1) << 3 | 6);

    if (pio) {
        auto addr = Kmem::ptr_to_phys (pio);
        write (Encoding::BITMAP_IO_A, addr);
        write (Encoding::BITMAP_IO_B, addr + PAGE_SIZE);
    }

    if (msr)
        write (Encoding::BITMAP_MSR, Kmem::ptr_to_phys (msr));

    write (Encoding::HOST_SEL_CS, static_cast<uint16>(SEL_KERN_CODE));
    write (Encoding::HOST_SEL_SS, static_cast<uint16>(SEL_KERN_DATA));
    write (Encoding::HOST_SEL_DS, static_cast<uint16>(SEL_KERN_DATA));
    write (Encoding::HOST_SEL_ES, static_cast<uint16>(SEL_KERN_DATA));
    write (Encoding::HOST_SEL_TR, static_cast<uint16>(SEL_TSS_RUN));

    write (Encoding::HOST_PAT,  Msr::read (Msr::IA32_PAT));
    write (Encoding::HOST_EFER, Msr::read (Msr::IA32_EFER));

    write (Encoding::PIN_CONTROLS, (pin | ctrl_pin.set) & ctrl_pin.clr);
    write (Encoding::EXI_CONTROLS, (exi | ctrl_exi.set) & ctrl_exi.clr);
    write (Encoding::ENT_CONTROLS, (ent | ctrl_ent.set) & ctrl_ent.clr);

    write (Encoding::EXI_MSR_ST_CNT, 0);
    write (Encoding::EXI_MSR_LD_CNT, 0);
    write (Encoding::ENT_MSR_LD_CNT, 0);

    write (Encoding::HOST_CR3, cr3);
    write (Encoding::HOST_CR0, get_cr0() | CR0_TS);
    write (Encoding::HOST_CR4, get_cr4());

    write (Encoding::HOST_BASE_TR,   reinterpret_cast<uintptr_t>(&Tss::run));
    write (Encoding::HOST_BASE_GDTR, reinterpret_cast<uintptr_t>(Gdt::gdt));
    write (Encoding::HOST_BASE_IDTR, reinterpret_cast<uintptr_t>(Idt::idt));

    write (Encoding::HOST_SYSENTER_CS,  0);
    write (Encoding::HOST_SYSENTER_ESP, 0);
    write (Encoding::HOST_SYSENTER_EIP, 0);

    write (Encoding::HOST_RSP, rsp);
    write (Encoding::HOST_RIP, reinterpret_cast<uintptr_t>(&entry_vmx));
}

void Vmcs::init()
{
    if (!Cpu::feature (Cpu::Feature::VMX) || (Msr::read (Msr::IA32_FEATURE_CONTROL) & 0x5) != 0x5)
        return;

    if (!Acpi::resume) {

        fix_cr0_set =  Msr::read (Msr::IA32_VMX_CR0_FIXED0);
        fix_cr0_clr = ~Msr::read (Msr::IA32_VMX_CR0_FIXED1);
        fix_cr4_set =  Msr::read (Msr::IA32_VMX_CR4_FIXED0);
        fix_cr4_clr = ~Msr::read (Msr::IA32_VMX_CR4_FIXED1);

        fix_cr0_clr |= CR0_CD | CR0_NW;

        basic.val       = Msr::read (Msr::IA32_VMX_BASIC);
        ctrl_exi.val    = Msr::read (basic.ctrl ? Msr::IA32_VMX_TRUE_EXIT  : Msr::IA32_VMX_CTRL_EXIT);
        ctrl_ent.val    = Msr::read (basic.ctrl ? Msr::IA32_VMX_TRUE_ENTRY : Msr::IA32_VMX_CTRL_ENTRY);
        ctrl_pin.val    = Msr::read (basic.ctrl ? Msr::IA32_VMX_TRUE_PIN   : Msr::IA32_VMX_CTRL_PIN);
        ctrl_cpu[0].val = Msr::read (basic.ctrl ? Msr::IA32_VMX_TRUE_PROC1 : Msr::IA32_VMX_CTRL_PROC1);
        ctrl_cpu[1].val = ctrl_cpu[0].clr & CPU_SECONDARY ? Msr::read (Msr::IA32_VMX_CTRL_PROC2) : 0;
        ctrl_cpu[2].val = ctrl_cpu[0].clr & CPU_TERTIARY  ? Msr::read (Msr::IA32_VMX_CTRL_PROC3) : 0;

        if (has_ept() || has_vpid())
            ept_vpid.val = Msr::read (Msr::IA32_VMX_EPT_VPID);
        if (has_ept())
            Eptp::set_leaf_max (static_cast<unsigned>(bit_scan_reverse (ept_vpid.super) + 2));
        if (has_mbec())
            Ept::mbec = true;
        if (has_urg())
            fix_cr0_set &= ~(CR0_PG | CR0_PE);

        ctrl_cpu[0].set |= CPU_HLT | CPU_IO | CPU_SECONDARY;
        ctrl_cpu[1].set |= CPU_MBEC | CPU_URG | CPU_VPID | CPU_EPT;

        if (!ept_vpid.invept)
            ctrl_cpu[1].clr &= ~(CPU_EPT | CPU_URG);
        if (Cmdline::novpid || !ept_vpid.invvpid)
            ctrl_cpu[1].clr &= ~CPU_VPID;

        if (!(root = new Vmcs))
            return;

        Hip::set_feature (Hip_arch::Feature::VMX);
    }

    set_cr0 ((get_cr0() & ~fix_cr0_clr) | fix_cr0_set);
    set_cr4 ((get_cr4() & ~fix_cr4_clr) | fix_cr4_set);

    vmxon();

    trace (TRACE_VIRT, "VMCS: %#010lx REV:%#x EPT:%u URG:%u VNMI:%u VPID:%u MBEC:%u", Kmem::ptr_to_phys (root), root->rev, has_ept(), has_urg(), has_vnmi(), has_vpid(), has_mbec());
}

void Vmcs::fini()
{
    // FIXME: Must clear every VMCS on this core
    if (current)
        current->clear();

    if (root)
        vmxoff();
}
