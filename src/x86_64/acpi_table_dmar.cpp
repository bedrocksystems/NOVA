/*
 * Advanced Configuration and Power Interface (ACPI)
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

#include "acpi_table_dmar.hpp"
#include "cmdline.hpp"
#include "hip.hpp"
#include "hpet.hpp"
#include "ioapic.hpp"
#include "pci.hpp"
#include "pd.hpp"
#include "smmu.hpp"
#include "stdio.hpp"

void Acpi_table_dmar::Remapping_drhd::parse() const
{
    auto smmu = new Smmu (phys);

    if (flags & Flags::INCLUDE_PCI_ALL)
        Pci::claim_all (smmu);

    auto addr = reinterpret_cast<uintptr_t>(this);

    for (auto a = addr + sizeof (*this); a < addr + length; ) {

        auto s = reinterpret_cast<Scope const *>(a);

        switch (s->type) {
            case Scope::PCI_EP:
            case Scope::PCI_SH: Pci::claim_dev (smmu, s->dev()); break;
            case Scope::IOAPIC: Ioapic::claim_dev (s->dev(), s->id); break;
            case Scope::HPET: Hpet::claim_dev (s->dev(), s->id); break;
            default: break;
        }

        trace (TRACE_FIRM | TRACE_PARSE, "DRHD: %#llx Scope Type %u Device %02x:%02x.%x", phys, s->type, s->b, s->d, s->f);

        a += s->length;
    }
}

void Acpi_table_dmar::Remapping_rmrr::parse() const
{
    for (uint64 hpa = base & ~OFFS_MASK; hpa < limit; hpa += PAGE_SIZE)
        Pd::kern.dpt.update (hpa, hpa, 0, Paging::Permissions (Paging::W | Paging::R), Memattr::Cacheability::MEM_WB, Memattr::Shareability::NONE);

    auto addr = reinterpret_cast<uintptr_t>(this);

    for (auto a = addr + sizeof (*this); a < addr + length; ) {

        auto s = reinterpret_cast<Scope const *>(a);

        Smmu *smmu = nullptr;

        switch (s->type) {
            case Scope::PCI_EP: smmu = Pci::find_smmu (s->dev()); break;
            default: break;
        }

        if (smmu)
            smmu->configure (&Pd::kern, Space::Index::DMA_HST, s->dev());

        trace (TRACE_FIRM | TRACE_PARSE, "RMRR: %#llx-%#llx Scope Type %u Device %02x:%02x.%x", base, limit, s->type, s->b, s->d, s->f);

        a += s->length;
    }
}

void Acpi_table_dmar::parse() const
{
    if (!Cmdline::iommu)
        return;

    auto addr = reinterpret_cast<uintptr_t>(this);

    for (auto a = addr + sizeof (*this); a < addr + length; ) {

        auto r = reinterpret_cast<Remapping const *>(a);

        switch (r->type) {
            case Remapping::DRHD: static_cast<Remapping_drhd const *>(r)->parse(); break;
            case Remapping::RMRR: static_cast<Remapping_rmrr const *>(r)->parse(); break;
            default: break;
        }

        a += r->length;
    }

    Smmu::enable (flags);

    Hip::hip->set_feature (Hip::FEAT_IOMMU);
}
