/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "bits.hpp"
#include "ec.hpp"
#include "elf.hpp"
#include "entry.hpp"
#include "hip.hpp"
#include "rcu.hpp"
#include "stdio.hpp"
#include "svm.hpp"
#include "vmx.hpp"

INIT_PRIORITY (PRIO_SLAB)
Slab_cache Ec::cache (sizeof (Ec), 32);

Ec *Ec::current, *Ec::fpowner;

// Constructors
Ec::Ec (Pd *own, void (*f)(), unsigned c) : Kobject (Kobject::Type::EC, Kobject::Subtype::EC_GLOBAL), cont (f), utcb (nullptr), pd (own), cpu (static_cast<uint16>(c)), glb (true), evt (0), timeout (this)
{
    trace (TRACE_SYSCALL, "EC:%p created (PD:%p Kernel)", this, own);
}

Ec::Ec (Pd *, mword, Pd *p, void (*f)(), unsigned c, unsigned e, mword u, mword s) : Kobject (Kobject::Type::EC, u ? (f ? Kobject::Subtype::EC_GLOBAL : Kobject::Subtype::EC_LOCAL) : Kobject::Subtype::EC_VCPU_REAL), cont (f), pd (p), cpu (static_cast<uint16>(c)), glb (!!f), evt (e), timeout (this)
{
    // Make sure we have a PTAB for this CPU in the PD
    pd->Space_mem::init (c);

    if (u) {

        (glb ? exc_regs().rsp : exc_regs().sp()) = s;

        utcb = new Utcb;

        pd->Space_mem::update (u, Kmem::ptr_to_phys (utcb), 0, Paging::Permissions (Paging::R | Paging::W | Paging::U), Memattr::Cacheability::MEM_WB, Memattr::Shareability::INNER);

        exc_regs().set_ep (NUM_EXC - 2);

        trace (TRACE_SYSCALL, "EC:%p created (PD:%p CPU:%#x UTCB:%#lx ESP:%lx EVT:%#x)", this, p, c, u, s, e);

    } else {

        exc_regs().set_ep (NUM_VMI - 2);

        if (Hip::hip->feature() & Hip::FEAT_VMX) {

            regs.vmcs = new Vmcs;
            regs.vmcs->init (pd->Space_pio::bmp_gst(),
                             pd->Space_msr::bmp_gst(),
                             reinterpret_cast<uintptr_t>(&sys_regs() + 1),
                             Kmem::ptr_to_phys (pd->loc[c].root_init (false)),
                             Kmem::ptr_to_phys (pd->ept.root_init (false)),
                             pd->vpid());

//          regs.nst_ctrl<Vmcs>();
            regs.vmcs->clear();
            cont = send_msg<ret_user_vmresume>;
            trace (TRACE_SYSCALL, "EC:%p created (PD:%p VMCS:%p)", this, p, regs.vmcs);

        } else if (Hip::hip->feature() & Hip::FEAT_SVM) {

            sys_regs().rax = Kmem::ptr_to_phys (regs.vmcb = new Vmcb (0, // FIXME: pd->Space_pio::walk(),
                                                                      Kmem::ptr_to_phys (pd->npt.root_init (false))));

//          regs.nst_ctrl<Vmcb>();
            cont = send_msg<ret_user_vmrun>;
            trace (TRACE_SYSCALL, "EC:%p created (PD:%p VMCB:%p)", this, p, regs.vmcb);
        }
    }
}

void Ec::handle_hazard (mword hzd, void (*func)())
{
    if (hzd & HZD_RCU)
        Rcu::quiet();

    if (hzd & HZD_SCHED) {
        current->cont = func;
        Sc::schedule();
    }

    if (hzd & HZD_RECALL) {
        current->clr_hazard (HZD_RECALL);

        if (func == ret_user_vmresume) {
            current->exc_regs().set_ep (NUM_VMI - 1);
            send_msg<ret_user_vmresume>();
        }

        if (func == ret_user_vmrun) {
            current->exc_regs().set_ep (NUM_VMI - 1);
            send_msg<ret_user_vmrun>();
        }

        if (func == ret_user_sysexit)
            current->redirect_to_iret();

        current->exc_regs().set_ep (NUM_EXC - 1);
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_TSC) {
        current->clr_hazard (HZD_TSC);

        if (func == ret_user_vmresume) {
            current->regs.vmcs->make_current();
            Vmcs::write (Vmcs::Encoding::TSC_OFFSET, current->regs.exc.offset_tsc);
        } else
            current->regs.vmcb->tsc_offset = current->regs.exc.offset_tsc;
    }

    if (hzd & HZD_FPU)
        if (current != fpowner)
            Fpu::disable();
}

void Ec::ret_user_sysexit()
{
    mword hzd = (Cpu::hazard | current->hazard) & (HZD_RECALL | HZD_RCU | HZD_FPU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_sysexit);

    asm volatile ("lea %0, %%rsp;" EXPAND (LOAD_GPR) "mov %%r11, %%rsp; mov $0x202, %%r11; sysretq" : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::ret_user_iret()
{
    mword hzd = (Cpu::hazard | current->hazard) & (HZD_RECALL | HZD_RCU | HZD_FPU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_iret);

    asm volatile ("lea %0, %%rsp;" EXPAND (LOAD_GPR IRET) : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::ret_user_vmresume()
{
    mword hzd = (Cpu::hazard | current->hazard) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmresume);

    current->regs.vmcs->make_current();

    if (EXPECT_FALSE (Pd::current->gtlb.tst (Cpu::id))) {
        Pd::current->gtlb.clr (Cpu::id);
        Pd::current->ept.invalidate();
    }

    if (EXPECT_FALSE (get_cr2() != current->exc_regs().cr2))
        set_cr2 (current->exc_regs().cr2);

    asm volatile ("lea %0, %%rsp;" EXPAND (LOAD_GPR)
                  "vmresume;"
                  "vmlaunch;"
                  "mov %1, %%rsp;"
                  : : "m" (current->regs), "i" (MMAP_CPU_STCK + PAGE_SIZE) : "memory");

    trace (0, "VM entry failed with error %#x", Vmcs::read<uint32> (Vmcs::Encoding::VMX_INST_ERROR));

    die ("VMENTRY");
}

void Ec::ret_user_vmrun()
{
    mword hzd = (Cpu::hazard | current->hazard) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmrun);

    if (EXPECT_FALSE (Pd::current->gtlb.tst (Cpu::id))) {
        Pd::current->gtlb.clr (Cpu::id);
        current->regs.vmcb->tlb_control = 1;
    }

    asm volatile ("lea %0, %%rsp;" EXPAND (LOAD_GPR)
                  "clgi;"
                  "sti;"
                  "vmload;"
                  "vmrun;"
                  "vmsave;"
                  EXPAND (SAVE_GPR)
                  "mov %1, %%rax;"
                  "mov %2, %%rsp;"
                  "vmload;"
                  "cli;"
                  "stgi;"
                  "jmp svm_handler;"
                  : : "m" (current->regs), "m" (Vmcb::root), "i" (MMAP_CPU_STCK + PAGE_SIZE) : "memory");

    UNREACHED;
}

void Ec::idle()
{
    for (;;) {

        mword hzd = Cpu::hazard & (HZD_RCU | HZD_SCHED);
        if (EXPECT_FALSE (hzd))
            handle_hazard (hzd, idle);

        Cpu::halt();
    }
}

void Ec::root_invoke()
{
    auto e = static_cast<Eh const *>(Hptp::map (Hip::root_addr));
    if (!Hip::root_addr || !e->valid (Eh::ELF_MACHINE))
        die ("No ELF");

#if 0   // FIXME
    current->regs.p0() = Cpu::id;
    current->regs.sp() = USER_ADDR - PAGE_SIZE;
    current->regs.ip() = e->entry;
    auto c = __atomic_load_n (&e->ph_count, __ATOMIC_RELAXED);
    auto p = static_cast<Ph const *>(Hpt::remap (Hip::root_addr + __atomic_load_n (&e->ph_offset, __ATOMIC_RELAXED)));

    for (unsigned i = 0; i < c; i++, p++) {

        if (p->type == 1) {

            unsigned attr = !!(p->flags & 0x4) << 0 |   // R
                            !!(p->flags & 0x2) << 1 |   // W
                            !!(p->flags & 0x1) << 2;    // X

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != (p->f_offs + Hip::root_addr) % PAGE_SIZE)
                die ("Bad ELF");

            mword phys = align_dn (p->f_offs + Hip::root_addr, PAGE_SIZE);
            mword virt = align_dn (p->v_addr, PAGE_SIZE);
            mword size = align_up (p->v_addr + p->f_size, PAGE_SIZE) - virt;

            for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o)
                Pd::current->delegate<Space_mem>(&Pd::kern, phys >> PAGE_BITS, virt >> PAGE_BITS, (o = min (max_order (phys, size), max_order (virt, size))) - PAGE_BITS, attr);
        }
    }

    // Map hypervisor information page
    Pd::current->delegate<Space_mem>(&Pd::kern, Kmem::ptr_to_phys (&PAGE_H) >> PAGE_BITS, (USER_ADDR - PAGE_SIZE) >> PAGE_BITS, 0, 1);
#endif

    current->pd->Space_obj::insert (Space_obj::num - 1, Capability (&Pd::kern,   static_cast<unsigned>(Capability::Perm_pd::CTRL)));
    current->pd->Space_obj::insert (Space_obj::num - 2, Capability (current->pd, static_cast<unsigned>(Capability::Perm_pd::DEFINED)));
    current->pd->Space_obj::insert (Space_obj::num - 3, Capability (Ec::current, static_cast<unsigned>(Capability::Perm_ec::DEFINED)));
    current->pd->Space_obj::insert (Space_obj::num - 4, Capability (Sc::current, static_cast<unsigned>(Capability::Perm_sc::DEFINED)));

    Console::flush();

    ret_user_sysexit();
}

void Ec::die (char const *reason, Exc_regs *r)
{
    if (current->utcb || current->pd == &Pd::kern)
        trace (0, "Killed EC:%p SC:%p V:%#lx CS:%#lx EIP:%#lx CR2:%#lx ERR:%#lx (%s)",
               current, Sc::current, r->vec, r->cs, r->rip, r->cr2, r->err, reason);
    else
        trace (0, "Killed EC:%p SC:%p V:%#lx (%s)",
               current, Sc::current, r->vec, reason);

    Ec *ec = current->rcap;

    if (ec)
        ec->cont = ec->cont == ret_user_sysexit ? static_cast<void (*)()>(sys_finish<Status::ABORTED>) : dead;

    reply (dead);
}
