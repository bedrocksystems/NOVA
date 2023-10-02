/*
 * Advanced Configuration and Power Interface (ACPI)
 *
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

#include "acpi_table_dbg2.hpp"
#include "stdio.hpp"

void Acpi_table_dbg2::parse() const
{
    auto const ptr { reinterpret_cast<uintptr_t>(this) };

    for (auto p { ptr + info_off }; p < ptr + table.header.length; ) {

        auto const i { reinterpret_cast<Info const *>(p) };
        auto const r { reinterpret_cast<Acpi_gas const *>(p + i->regs_off) };

        if (i->type != Debug::Type::SERIAL)
            continue;

        Console::bind (r, i->subtype);

        trace (TRACE_FIRM, "DBG2: Console %#x at %u:%#lx:%u", std::to_underlying (i->subtype), std::to_underlying (r->asid), r->addr, r->bits);

        p += i->length;
    }
}
