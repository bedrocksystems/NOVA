/*
 * Generic Interrupt Controller: Redistributor (GICR)
 *
 * Copyright (C) 2019-2020 Udo Steinberg, BedRock Systems, Inc.
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

#include "coresight.hpp"
#include "intid.hpp"
#include "memory.hpp"
#include "types.hpp"

class Gicr : private Coresight, private Intid
{
    private:
        enum class Register32
        {
            CTLR        =  0x00000, // -- v3 rw Control Register
            IIDR        =  0x00004, // -- v3 ro Implementer Identification Register
            STATUSR     =  0x00010, // -- v3 rw Error Reporting Status Register
            WAKER       =  0x00014, // -- v3 rw Wake Register
            SYNCR       =  0x000c0, // -- v3 ro Synchronize Register
            IGROUPR0    =  0x10080, // -- v3 rw Interrupt Group Register 0
            ISENABLER0  =  0x10100, // -- v3 rw Interrupt Set-Enable Register 0
            ICENABLER0  =  0x10180, // -- v3 rw Interrupt Clr-Enable Register 0
            ISPENDR0    =  0x10200, // -- v3 rw Interrupt Set-Pending Register 0
            ICPENDR0    =  0x10280, // -- v3 rw Interrupt Clr-Pending Register 0
            ISACTIVER0  =  0x10300, // -- v3 rw Interrupt Set-Active Register 0
            ICACTIVER0  =  0x10380, // -- v3 rw Interrupt Clr-Active Register 0
        };

        enum class Register64
        {
            TYPER       =  0x00008, // -- v3 ro Type Register
            SETLPIR     =  0x00040, // -- v3 wo Set LPI Pending Register
            CLRLPIR     =  0x00048, // -- v3 wo Clr LPI Pending Register
            PROPBASER   =  0x00070, // -- v3 rw Properties Base Address Register
            PENDBASER   =  0x00078, // -- v3 rw LPI Pending Table Base Address Register
            INVLPIR     =  0x000a0, // -- v3 wo Invalidate LPI Register
            INVALLR     =  0x000b0, // -- v3 wo Invalidate All Register
        };

        enum class Array32
        {
            IPRIORITYR  =  0x10400, // -- v3 rw Interrupt Priority Registers
            ICFGR       =  0x10c00, // -- v3 rw SGI/PPI Configuration Registers
        };

        static inline auto read  (Register32 r)             { return *reinterpret_cast<uint32 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r)); }
        static inline auto read  (Register64 r)             { return *reinterpret_cast<uint64 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r)); }
        static inline auto read  (Array32 r, unsigned n)    { return *reinterpret_cast<uint32 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r) + n * sizeof (uint32)); }

        static inline void write (Register32 r,          uint32 v) { *reinterpret_cast<uint32 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r)) = v; }
        static inline void write (Register64 r,          uint64 v) { *reinterpret_cast<uint64 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r)) = v; }
        static inline void write (Array32 r, unsigned n, uint32 v) { *reinterpret_cast<uint32 volatile *>(DEV_LOCAL_GICR + static_cast<mword>(r) + n * sizeof (uint32)) = v; }

        static void wait_rwp();

    public:
        static void init();

        static void conf (unsigned, bool);
        static void mask (unsigned, bool);
};
