/*
 * System-Call Interface
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
#include "counter.hpp"
#include "ec_arch.hpp"
#include "hazards.hpp"
#include "interrupt.hpp"
#include "lowlevel.hpp"
#include "pd_kern.hpp"
#include "pt.hpp"
#include "sm.hpp"
#include "smmu.hpp"
#include "stdio.hpp"
#include "syscall.hpp"
#include "syscall_tmpl.hpp"
#include "utcb.hpp"

Ec::cont_t const Ec::syscall[16] =
{
    &sys_ipc_call,
    &sys_ipc_reply,
    &sys_create_pd,
    &sys_create_ec,
    &sys_create_sc,
    &sys_create_pt,
    &sys_create_sm,
    &sys_ctrl_pd,
    &sys_ctrl_ec,
    &sys_ctrl_sc,
    &sys_ctrl_pt,
    &sys_ctrl_sm,
    &sys_ctrl_pm,
    &sys_assign_int,
    &sys_assign_dev,
    &sys_finish<Status::BAD_HYP>,
};

void Ec::recv_kern (Ec *const self)
{
    auto ec = self->caller;

    assert (ec);

    assert (self);
    assert (self->utcb);
    assert (self->subtype == Kobject::Subtype::EC_LOCAL);
    assert (self->cont == recv_kern);

    Mtd_arch mtd = static_cast<Sys_ipc_reply *>(self->sys_regs())->mtd_a();

    static_cast<Ec_arch *>(ec)->state_load (self, mtd);

    Ec_arch::ret_user_hypercall (self);
}

void Ec::recv_user (Ec *const self)
{
    auto ec = self->caller;

    assert (ec);
    assert (ec->utcb);
    assert (ec->cont == Ec_arch::ret_user_hypercall);

    assert (self);
    assert (self->utcb);
    assert (self->subtype == Kobject::Subtype::EC_LOCAL);
    assert (self->cont == recv_user);

    Mtd_user mtd = static_cast<Sys_ipc_reply *>(self->sys_regs())->mtd_u();

    ec->utcb->copy (mtd, self->utcb);

    Ec_arch::ret_user_hypercall (self);
}

void Ec::rendezvous (Ec *const ec, cont_t c, cont_t e, uintptr_t ip, uintptr_t id, uintptr_t mtd)
{
    if (EXPECT_FALSE (ec->cont))
        return;

    cont = c;
    set_partner (ec);

    ec->cont = e;
    ec->regs.ip() = ip;
    ec->regs.p0() = id;
    ec->regs.p1() = mtd;

    static_cast<Ec_arch *>(ec)->make_current();
}

void Ec::reply (cont_t c)
{
    cont = c;

    auto ec = caller;

    if (EXPECT_TRUE (ec)) {

        assert (subtype == Kobject::Subtype::EC_LOCAL);

        if (EXPECT_TRUE (ec->clr_partner()))
            static_cast<Ec_arch *>(ec)->make_current();

        Scheduler::get_current()->get_ec()->activate();
    }

    Scheduler::schedule (true);
}

template <Ec::cont_t C>
void Ec::send_msg (Ec *const self)
{
    auto r = self->exc_regs();

    auto cap = self->pd->Space_obj::lookup (self->evt + r->ep());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pt::EVENT)))
        self->kill ("PT not found");

    auto pt = static_cast<Pt *>(cap.obj());
    auto ec = pt->ec;

    if (EXPECT_FALSE (self->cpu != ec->cpu))
        self->kill ("PT wrong CPU");

    assert (ec->subtype == Kobject::Subtype::EC_LOCAL);

    self->rendezvous (ec, C, recv_kern, pt->ip, pt->get_id(), pt->get_mtd());

    self->help (ec, send_msg<C>);

    self->kill ("IPC Abort");
}

void Ec::sys_ipc_call (Ec *const self)
{
    auto r = static_cast<Sys_ipc_call *>(self->sys_regs());

    auto cap = self->pd->Space_obj::lookup (r->pt());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pt::CALL)))
        sys_finish<Status::BAD_CAP> (self);

    auto pt = static_cast<Pt *>(cap.obj());
    auto ec = pt->ec;

    if (EXPECT_FALSE (self->cpu != ec->cpu))
        sys_finish<Status::BAD_CPU> (self);

    assert (ec->subtype == Kobject::Subtype::EC_LOCAL);

    self->rendezvous (ec, Ec_arch::ret_user_hypercall, recv_user, pt->ip, pt->get_id(), r->mtd());

    if (EXPECT_FALSE (r->timeout()))
        sys_finish<Status::TIMEOUT> (self);

    self->help (ec, sys_ipc_call);

    sys_finish<Status::ABORTED> (self);
}

void Ec::sys_ipc_reply (Ec *const self)
{
    auto r = static_cast<Sys_ipc_reply *>(self->sys_regs());

    auto ec = self->caller;

    if (EXPECT_TRUE (ec)) {

        if (EXPECT_TRUE (ec->cont == Ec_arch::ret_user_hypercall)) {
            ec->regs.p1() = r->mtd_u();
            self->utcb->copy (r->mtd_u(), ec->utcb);
        }

        else if (EXPECT_FALSE (!static_cast<Ec_arch *>(ec)->state_save (self, r->mtd_a())))
            ec->set_hazard (HZD_ILLEGAL);
    }

    self->reply();
}

void Ec::sys_create_pd (Ec *const self)
{
    auto r = static_cast<Sys_create_pd *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s PD:%#lx", static_cast<void *>(self), __func__, r->sel());

    auto cap = self->pd->Space_obj::lookup (r->own());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::PD))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->own());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto pd = Pd::create();

    if (EXPECT_FALSE (!pd)) {
        trace (TRACE_ERROR, "%s: Insufficient MEM", __func__);
        sys_finish<Status::INS_MEM> (self);
    }

    auto s = self->pd->Space_obj::insert (r->sel(), Capability (pd, cap.prm()));

    if (EXPECT_TRUE (s == Status::SUCCESS))
        sys_finish<Status::SUCCESS> (self);

    pd->destroy();

    switch (s) {

        default:

        case Status::BAD_CAP:
            trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::BAD_CAP> (self);

        case Status::INS_MEM:
            trace (TRACE_ERROR, "%s: Insufficient MEM for CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::INS_MEM> (self);
    }
}

void Ec::sys_create_ec (Ec *const self)
{
    auto r = static_cast<Sys_create_ec *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s EC:%#lx PD:%#lx CPU:%#x UTCB:%#lx SP:%#lx EB:%#lx", static_cast<void *>(self), __func__, r->sel(), r->own(), r->cpu(), r->utcb(), r->sp(), r->eb());

    if (EXPECT_FALSE (r->cpu() >= Cpu::count)) {
        trace (TRACE_ERROR, "%s: Invalid CPU (%u)", __func__, r->cpu());
        sys_finish<Status::BAD_CPU> (self);
    }

    if (EXPECT_FALSE (r->utcb() >= Space_mem::num << PAGE_BITS)) {
        trace (TRACE_ERROR, "%s: Invalid UTCB address (%#lx)", __func__, r->utcb());
        sys_finish<Status::BAD_PAR> (self);
    }

    if (EXPECT_FALSE (r->vcpu() && !vcpu_supported())) {
        trace (TRACE_ERROR, "%s: VCPUs not supported", __func__);
        sys_finish<Status::BAD_FTR> (self);
    }

    auto cap = self->pd->Space_obj::lookup (r->own());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::EC_PT_SM))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->own());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto pd = static_cast<Pd *>(cap.obj());

    auto ec = r->vcpu() ? Ec::create (pd, r->fpu(), r->cpu(), r->eb(), r->type()) :
                          Ec::create (pd, r->fpu(), r->cpu(), r->eb(), r->utcb(), r->sp(), r->type() ? send_msg<Ec_arch::ret_user_exception> : nullptr);

    if (EXPECT_FALSE (!ec)) {
        trace (TRACE_ERROR, "%s: Insufficient MEM", __func__);
        sys_finish<Status::INS_MEM> (self);
    }

    auto s = self->pd->Space_obj::insert (r->sel(), Capability (ec, static_cast<unsigned>(Capability::Perm_ec::DEFINED)));

    if (EXPECT_TRUE (s == Status::SUCCESS))
        sys_finish<Status::SUCCESS> (self);

    ec->destroy();

    switch (s) {

        default:

        case Status::BAD_CAP:
            trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::BAD_CAP> (self);

        case Status::INS_MEM:
            trace (TRACE_ERROR, "%s: Insufficient MEM for CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::INS_MEM> (self);
    }
}

void Ec::sys_create_sc (Ec *const self)
{
    auto r = static_cast<Sys_create_sc *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SC:%#lx EC:%#lx P:%u B:%u", static_cast<void *>(self), __func__, r->sel(), r->ec(), r->prio(), r->budget());

    auto cap = self->pd->Space_obj::lookup (r->own());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::SC))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->own());
        sys_finish<Status::BAD_CAP> (self);
    }

    cap = self->pd->Space_obj::lookup (r->ec());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_ec::BIND_SC))) {
        trace (TRACE_ERROR, "%s: Bad EC CAP (%#lx)", __func__, r->ec());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto ec = static_cast<Ec *>(cap.obj());

    if (EXPECT_FALSE (ec->subtype == Kobject::Subtype::EC_LOCAL)) {
        trace (TRACE_ERROR, "%s: Cannot bind SC", __func__);
        sys_finish<Status::BAD_CAP> (self);
    }

    if (EXPECT_FALSE (!r->prio() || !r->budget())) {
        trace (TRACE_ERROR, "%s: Invalid prio/budget", __func__);
        sys_finish<Status::BAD_PAR> (self);
    }

    auto sc = Sc::create (ec->cpu, ec, r->prio(), r->budget());

    if (EXPECT_FALSE (!sc)) {
        trace (TRACE_ERROR, "%s: Insufficient MEM", __func__);
        sys_finish<Status::INS_MEM> (self);
    }

    auto s = self->pd->Space_obj::insert (r->sel(), Capability (sc, static_cast<unsigned>(Capability::Perm_sc::DEFINED)));

    if (EXPECT_TRUE (s == Status::SUCCESS)) {
        Scheduler::unblock (sc);
        sys_finish<Status::SUCCESS> (self);
    }

    sc->destroy();

    switch (s) {

        default:

        case Status::BAD_CAP:
            trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::BAD_CAP> (self);

        case Status::INS_MEM:
            trace (TRACE_ERROR, "%s: Insufficient MEM for CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::INS_MEM> (self);
    }
}

void Ec::sys_create_pt (Ec *const self)
{
    auto r = static_cast<Sys_create_pt *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s PT:%#lx EC:%#lx IP:%#lx", static_cast<void *>(self), __func__, r->sel(), r->ec(), r->ip());

    auto cap = self->pd->Space_obj::lookup (r->own());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::EC_PT_SM))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->own());
        sys_finish<Status::BAD_CAP> (self);
    }

    cap = self->pd->Space_obj::lookup (r->ec());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_ec::BIND_PT))) {
        trace (TRACE_ERROR, "%s: Bad EC CAP (%#lx)", __func__, r->ec());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto ec = static_cast<Ec *>(cap.obj());

    if (EXPECT_FALSE (ec->subtype != Kobject::Subtype::EC_LOCAL)) {
        trace (TRACE_ERROR, "%s: Cannot bind PT", __func__);
        sys_finish<Status::BAD_CAP> (self);
    }

    auto pt = Pt::create (ec, r->ip());

    if (EXPECT_FALSE (!pt)) {
        trace (TRACE_ERROR, "%s: Insufficient MEM", __func__);
        sys_finish<Status::INS_MEM> (self);
    }

    auto s = self->pd->Space_obj::insert (r->sel(), Capability (pt, static_cast<unsigned>(Capability::Perm_pt::DEFINED)));

    if (EXPECT_TRUE (s == Status::SUCCESS))
        sys_finish<Status::SUCCESS> (self);

    pt->destroy();

    switch (s) {

        default:

        case Status::BAD_CAP:
            trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::BAD_CAP> (self);

        case Status::INS_MEM:
            trace (TRACE_ERROR, "%s: Insufficient MEM for CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::INS_MEM> (self);
    }
}

void Ec::sys_create_sm (Ec *const self)
{
    auto r = static_cast<Sys_create_sm *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SM:%#lx CNT:%llu", static_cast<void *>(self), __func__, r->sel(), r->cnt());

    auto cap = self->pd->Space_obj::lookup (r->own());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::EC_PT_SM))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->own());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto sm = Sm::create (r->cnt());

    if (EXPECT_FALSE (!sm)) {
        trace (TRACE_ERROR, "%s: Insufficient MEM", __func__);
        sys_finish<Status::INS_MEM> (self);
    }

    auto s = self->pd->Space_obj::insert (r->sel(), Capability (sm, static_cast<unsigned>(Capability::Perm_sm::DEFINED)));

    if (EXPECT_TRUE (s == Status::SUCCESS))
        sys_finish<Status::SUCCESS> (self);

    sm->destroy();

    switch (s) {

        default:

        case Status::BAD_CAP:
            trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::BAD_CAP> (self);

        case Status::INS_MEM:
            trace (TRACE_ERROR, "%s: Insufficient MEM for CAP (%#lx)", __func__, r->sel());
            sys_finish<Status::INS_MEM> (self);
    }
}

void Ec::sys_ctrl_pd (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_pd *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SPD:%#lx DPD:%#lx ST:%u SI:%u SRC:%#lx DST:%#lx ORD:%u PMM:%#x CA:%u SH:%u", static_cast<void *>(self), __func__, r->spd(), r->dpd(), static_cast<unsigned>(r->st()), static_cast<unsigned>(r->si()), r->src(), r->dst(), r->ord(), r->pmm(), static_cast<unsigned>(r->ca()), static_cast<unsigned>(r->sh()));

    if (EXPECT_FALSE ((r->src() | r->dst()) & (BITN (r->ord()) - 1))) {
        trace (TRACE_ERROR, "%s: Unaligned selector", __func__);
        sys_finish<Status::BAD_PAR> (self);
    }

    auto cap = self->pd->Space_obj::lookup (r->spd());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::CTRL))) {
        trace (TRACE_ERROR, "%s: Bad SRC PD CAP (%#lx)", __func__, r->spd());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto src = static_cast<Pd *>(cap.obj());

    cap = self->pd->Space_obj::lookup (r->dpd());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::CTRL))) {
        trace (TRACE_ERROR, "%s: Bad DST PD CAP (%#lx)", __func__, r->dpd());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto dst = static_cast<Pd *>(cap.obj());

    if (EXPECT_FALSE (dst == &Pd_kern::nova())) {
        trace (TRACE_ERROR, "%s: Bad DST PD CAP (%#lx)", __func__, r->dpd());
        sys_finish<Status::BAD_CAP> (self);
    }

    Status s;

    switch (r->st()) {
        case Space::Type::OBJ: s = dst->delegate_obj (src, r->src(), r->dst(), r->ord(), r->pmm()); break;
        case Space::Type::MEM: s = dst->delegate_mem (src, r->src(), r->dst(), r->ord(), r->pmm(), r->si(), r->ca(), r->sh()); break;
        case Space::Type::PIO: s = dst->delegate_pio (src, r->src(), r->src(), r->ord(), r->pmm(), r->si()); break;
        case Space::Type::MSR: s = dst->delegate_msr (src, r->src(), r->src(), r->ord(), r->pmm(), r->si()); break;
    }

    switch (s) {
        case Status::SUCCESS: sys_finish<Status::SUCCESS> (self);
        case Status::INS_MEM: sys_finish<Status::INS_MEM> (self);
        default:              sys_finish<Status::BAD_PAR> (self);
    }
}

void Ec::sys_ctrl_ec (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_ec *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s EC:%#lx (%c)", static_cast<void *>(self), __func__, r->ec(), r->strong() ? 'S' : 'W');

    auto cap = self->pd->Space_obj::lookup (r->ec());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_ec::CTRL))) {
        trace (TRACE_ERROR, "%s: Bad EC CAP (%#lx)", __func__, r->ec());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto ec = static_cast<Ec *>(cap.obj());

    // Strong: Must wait for observation even if the hazard was set already
    if (r->strong()) {

        ec->set_hazard (HZD_RECALL);

        // Send IPI only if the EC is remote and current on its core
        if (Cpu::id != ec->cpu && Ec::remote_current (ec->cpu) == ec) {
            Cpu::preemption_enable();
            auto cnt = Counter::req[Interrupt::Request::RKE].get (ec->cpu);
            Interrupt::send_cpu (Interrupt::Request::RKE, ec->cpu);
            while (Counter::req[Interrupt::Request::RKE].get (ec->cpu) == cnt)
                pause();
            Cpu::preemption_disable();
        }

    // Weak: Send IPI only if the hazard was not set already and the EC is remote and current on its core
    } else if (!ec->tas_hazard (HZD_RECALL) && Cpu::id != ec->cpu && Ec::remote_current (ec->cpu) == ec)
        Interrupt::send_cpu (Interrupt::Request::RKE, ec->cpu);

    sys_finish<Status::SUCCESS> (self);
}

void Ec::sys_ctrl_sc (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_sc *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SC:%#lx", static_cast<void *>(self), __func__, r->sc());

    auto cap = self->pd->Space_obj::lookup (r->sc());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_sc::CTRL))) {
        trace (TRACE_ERROR, "%s: Bad SC CAP (%#lx)", __func__, r->sc());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto sc = static_cast<Sc *>(cap.obj());

    r->set_time_ticks (sc->get_used());

    sys_finish<Status::SUCCESS> (self);
}

void Ec::sys_ctrl_pt (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_pt *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s PT:%#lx ID:%#lx MTD:%#x", static_cast<void *>(self), __func__, r->pt(), r->id(), static_cast<unsigned>(r->mtd()));

    auto cap = self->pd->Space_obj::lookup (r->pt());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pt::CTRL))) {
        trace (TRACE_ERROR, "%s: Bad PT CAP (%#lx)", __func__, r->pt());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto pt = static_cast<Pt *>(cap.obj());

    pt->set_id (r->id());
    pt->set_mtd (r->mtd());

    sys_finish<Status::SUCCESS> (self);
}

void Ec::sys_ctrl_sm (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_sm *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SM:%#lx OP:%u", static_cast<void *>(self), __func__, r->sm(), r->op());

    auto cap = self->pd->Space_obj::lookup (r->sm());
    if (EXPECT_FALSE (!cap.validate (r->op() ? Capability::Perm_sm::CTRL_DN : Capability::Perm_sm::CTRL_UP))) {
        trace (TRACE_ERROR, "%s: Bad SM CAP (%#lx)", __func__, r->sm());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto sm = static_cast<Sm *>(cap.obj());

    if (r->op()) {          // Down

        auto id = sm->get_id();

        if (id != ~0U) {

            Interrupt::Config cfg = Interrupt::int_table[id].config;

            if (Cpu::id != cfg.cpu()) {
                trace (TRACE_ERROR, "%s: Invalid CPU (%u)", __func__, Cpu::id);
                sys_finish<Status::BAD_CPU> (self);
            }

            // Guest-assigned interrupts are deactivated by the guest
            if (!cfg.gst())
                Interrupt::deactivate (id);
        }

        sm->dn (self, r->zc(), r->time_ticks());

    } else if (!sm->up())   // Up
        sys_finish<Status::OVRFLOW> (self);

    sys_finish<Status::SUCCESS> (self);
}

void Ec::sys_ctrl_pm (Ec *const self)
{
    auto r = static_cast<Sys_ctrl_pm *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s OP:%u", static_cast<void *>(self), __func__, r->op());

    if (EXPECT_FALSE (self->pd != Pd::root)) {
        trace (TRACE_ERROR, "%s: Not Root PD", __func__);
        sys_finish<Status::BAD_HYP> (self);
    }

    switch (r->op()) {

        case 1:
            Acpi_fixed::Transition t { r->trans() };

            if (!Acpi_fixed::supported (t)) {
                trace (TRACE_ERROR, "%s: Unsupported Transition", __func__);
                sys_finish<Status::BAD_FTR> (self);
            }

            if (!Acpi::set_transition (t)) {
                trace (TRACE_ERROR, "%s: Concurrent operation aborted", __func__);
                sys_finish<Status::ABORTED> (self);
            }

            Interrupt::send_exc (Interrupt::Request::RKE);

            Cpu::hazard |= HZD_SLEEP;

            sys_finish<Status::SUCCESS> (self);
    }

    sys_finish<Status::BAD_PAR> (self);
}

void Ec::sys_assign_int (Ec *const self)
{
    auto r = static_cast<Sys_assign_int *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s SM:%#lx CPU:%u FLG:%#x", static_cast<void *>(self), __func__, r->sm(), r->cpu(), r->flags());

    if (EXPECT_FALSE (r->cpu() >= Cpu::count)) {
        trace (TRACE_ERROR, "%s: Invalid CPU (%u)", __func__, r->cpu());
        sys_finish<Status::BAD_CPU> (self);
    }

    auto cap = self->pd->Space_obj::lookup (r->sm());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_sm::ASSIGN))) {
        trace (TRACE_ERROR, "%s: Bad SM CAP (%#lx)", __func__, r->sm());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto id = static_cast<Sm *>(cap.obj())->get_id();

    if (EXPECT_FALSE (id == ~0U)) {
        trace (TRACE_ERROR, "%s: Bad IS CAP (%#lx)", __func__, r->sm());
        sys_finish<Status::BAD_CAP> (self);
    }

    uint32 msi_addr;
    uint16 msi_data;

    Interrupt::configure (id, Interrupt::Config (r->cpu(), r->dev(), r->flags()), msi_addr, msi_data);

    r->set_msi_addr (msi_addr);
    r->set_msi_data (msi_data);

    sys_finish<Status::SUCCESS> (self);
}

void Ec::sys_assign_dev (Ec *const self)
{
    auto r = static_cast<Sys_assign_dev *>(self->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p %s PD:%#lx SMMU:%#lx SI:%u DEV:%#lx", static_cast<void *>(self), __func__, r->pd(), r->smmu(), static_cast<unsigned>(r->si()), r->dev());

    if (EXPECT_FALSE (self->pd != Pd::root)) {
        trace (TRACE_ERROR, "%s: Not Root PD", __func__);
        sys_finish<Status::BAD_HYP> (self);
    }

    Smmu *smmu = Smmu::lookup (r->smmu());

    if (EXPECT_FALSE (!smmu)) {
        trace (TRACE_ERROR, "%s: Bad SMMU (%#lx)", __func__, r->smmu());
        sys_finish<Status::BAD_DEV> (self);
    }

    auto cap = self->pd->Space_obj::lookup (r->pd());
    if (EXPECT_FALSE (!cap.validate (Capability::Perm_pd::ASSIGN))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Status::BAD_CAP> (self);
    }

    auto pd = static_cast<Pd *>(cap.obj());

    if (!smmu->configure (pd, r->si(), r->dev())) {
        trace (TRACE_ERROR, "%s: Bad Parameter for SI/DEV", __func__);
        sys_finish<Status::BAD_PAR> (self);
    }

    sys_finish<Status::SUCCESS> (self);
}

template <Status S, bool T>
void Ec::sys_finish (Ec *const self)
{
    if (T)
        self->clr_timeout();

    self->regs.p0() = static_cast<uintptr_t> (S);

    Ec_arch::ret_user_hypercall (self);
}