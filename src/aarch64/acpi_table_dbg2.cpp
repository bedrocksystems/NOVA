/*
 * Advanced Configuration and Power Interface (ACPI)
 *
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

#include "acpi_gas.hpp"
#include "acpi_table_dbg2.hpp"
#include "console_uart_mmio_imx.hpp"
#include "console_uart_mmio_ns16550.hpp"
#include "console_uart_mmio_pl011.hpp"

void Acpi_table_dbg2::parse() const
{
    for (auto d { reinterpret_cast<uint8 const *>(this) + info_off }; d < reinterpret_cast<uint8 const *>(this) + table.header.length; ) {

        auto const i { reinterpret_cast<Info const *>(d) };
        auto const r { reinterpret_cast<Acpi_gas const *>(d + i->regs_off) };

        if (i->type != Info::Type::SERIAL || r->asid != Acpi_gas::Asid::MMIO)
            continue;

        switch (i->subtype) {

            case Info::Subtype::SERIAL_PL011:
                Console_uart_mmio_pl011::singleton.mmap (r->addr);
                break;

            case Info::Subtype::SERIAL_IMX:
                Console_uart_mmio_imx::singleton.mmap (r->addr);
                break;

            case Info::Subtype::SERIAL_NS16550_FULL:
            case Info::Subtype::SERIAL_NS16550_SUBSET:
            case Info::Subtype::SERIAL_NS16550_PARAM:
                Console_uart_mmio_ns16550::singleton.mmap (r->addr);
                break;

            default:
                break;
        }

        d += i->length;
    }
}
