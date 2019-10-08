/*
 * Bootstrap Code
 *
 * Copyright (C) 2019-2024 Udo Steinberg, BedRock Systems, Inc.
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

#include "compiler.hpp"
#include "cpu.hpp"
#include "interrupt.hpp"
#include "lowlevel.hpp"
#include "smmu.hpp"

extern "C" [[noreturn]]
void bootstrap (cpu_t c, unsigned e)
{
    Cpu::init (c, e);

    // Once initialized, each core can handle its assigned interrupts
    Interrupt::init();

    // Before cores leave the barrier into userland, the SMMU must be active
    if (Cpu::bsp)
        Smmu::initialize();

    // Barrier: wait for all CPUs to arrive here
    for (Cpu::online++; Cpu::online != Cpu::count; pause()) ;

    for (;;) {}
}
