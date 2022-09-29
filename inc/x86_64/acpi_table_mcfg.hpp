/*
 * Advanced Configuration and Power Interface (ACPI)
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

#include "acpi_table.hpp"

/*
 * Memory Mapped Configuration Space Description Table (MCFG)
 */
class Acpi_table_mcfg final
{
    private:
        Acpi_table  table;                              // 0
        uint32      reserved[2];                        // 36

        /*
         * Each Allocation struct is only 32bit (but not 64bit) aligned. This is because
         * the first struct is at offset n=44 and each struct has a size of 16.
         */
        struct Allocation
        {
            uint32      phys_base_lo, phys_base_hi;     //  0 + n
            uint16      seg;                            //  8 + n
            uint8       bus_s;                          // 10 + n
            uint8       bus_e;                          // 11 + n
            uint32      reserved;                       // 12 + n
                                                        // 16 + n
            void parse() const;
        };

    public:
        void parse() const;
};

static_assert (__is_standard_layout (Acpi_table_mcfg) && sizeof (Acpi_table_mcfg) == 44);
