/*
 * Bootstrap Code
 *
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

#include "compiler.hpp"
#include "cpu.hpp"
#include "lowlevel.hpp"
#include "interrupt.hpp"
#include "smmu.hpp"

extern "C" NORETURN
void bootstrap (unsigned i, unsigned e)
{
    Cpu::init (i, e);

    // Before cores leave the barrier into userland, the SMMU must be active
    if (Cpu::bsp)
        Smmu::initialize();

    // Barrier: wait for all CPUs to arrive here
    for (Cpu::online++; Cpu::online != Cpu::count; pause()) ;

    // Once all cores are up, we can route interrupts to them
    if (Cpu::bsp)
        Interrupt::init();

    for (;;) {}
}
