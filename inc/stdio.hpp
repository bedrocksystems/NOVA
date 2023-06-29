/*
 * Standard I/O
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#pragma once

#include "console.hpp"
#include "cpu.hpp"
#include "macros.hpp"
#include "memory.hpp"

static inline auto stackptr() { return reinterpret_cast<uintptr_t>(__builtin_frame_address (0)); }

#define trace(T,format,...)                         \
do {                                                \
    if (EXPECT_FALSE ((trace_mask & (T)) == (T)))   \
        Console::print ("[%3ld] " format, static_cast<long>(((stackptr() - 1) & ~OFFS_MASK (0)) == MMAP_CPU_DSTK ? ACCESS_ONCE (Cpu::id) : ~0UL), ## __VA_ARGS__);   \
} while (0)

/*
 * Definition of trace events
 */
enum {
    TRACE_CPU       = BIT  (0),
    TRACE_SMMU      = BIT  (1),
    TRACE_TIMR      = BIT  (2),
    TRACE_INTR      = BIT  (3),
    TRACE_VIRT      = BIT  (6),
    TRACE_FIRM      = BIT  (7),
    TRACE_DRTM      = BIT  (8),
    TRACE_ROOT      = BIT (11),
    TRACE_PTE       = BIT (12),
    TRACE_MEMORY    = BIT (13),
    TRACE_PCI       = BIT (14),
    TRACE_SCHEDULE  = BIT (16),
    TRACE_RCU       = BIT (20),
    TRACE_FPU       = BIT (23),
    TRACE_PERF      = BIT (24),
    TRACE_CONT      = BIT (25),
    TRACE_PARSE     = BIT (26),
    TRACE_CREATE    = BIT (27),
    TRACE_SYSCALL   = BIT (28),
    TRACE_EXCEPTION = BIT (29),
    TRACE_ERROR     = BIT (30),
    TRACE_KILL      = BIT (31),
};

/*
 * Enabled trace events
 */
constexpr auto trace_mask   { TRACE_CPU     |
                              TRACE_FPU     |
                              TRACE_PCI     |
                              TRACE_SMMU    |
                              TRACE_TIMR    |
                              TRACE_INTR    |
                              TRACE_VIRT    |
                              TRACE_FIRM    |
                              TRACE_DRTM    |
                              TRACE_ROOT    |
                              TRACE_PERF    |
                              TRACE_KILL    |
#ifdef DEBUG
                              TRACE_ERROR   |
#endif
                              0 };
