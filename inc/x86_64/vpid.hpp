/*
 * Virtual Processor Identifier (VPID)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#pragma once

#include "atomic.hpp"
#include "kmem.hpp"

class Invvpid final
{
    private:
        uint64 const vpid;
        uint64 const addr;

    public:
        enum Type
        {
            ADR = 0,    // Individual address
            SGL = 1,    // Single context
            ALL = 2,    // All contexts
            SRG = 3,    // Single context retaining globals
        };

        ALWAYS_INLINE
        inline Invvpid (uint16 v, uint64 a) : vpid (v), addr (a) {}
};

class Vpid final
{
    private:
        static Atomic<uint16> allocator CPULOCAL;

    public:
        // FIXME: Handle overflow
        static inline auto alloc (unsigned cpu)
        {
            return ++*Kmem::loc_to_glob (&allocator, cpu);
        }

        static inline void invalidate (Invvpid::Type t, uint16 vpid, uint64 addr = 0)
        {
            asm volatile ("invvpid %0, %1" : : "m" (Invvpid (vpid, addr)), "r" (static_cast<uintptr_t>(t)) : "cc");
        }
};
