/*
 * Initialization Code
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

#include "acpi.hpp"
#include "cmdline.hpp"
#include "console.hpp"
#include "cpu.hpp"
#include "extern.hpp"
#include "ptab_hpt.hpp"

extern "C" auto kern_ptab_setup (cpu_t cpu)
{
    return Cpu::remote_ptab (cpu);
}

extern "C" void preinit()
{
    if (!Acpi::resume)
        Cmdline::init();
}

extern "C" unsigned init()
{
    if (!Acpi::resume) {

        Buddy::init();

        for (auto func { CTORS_S }; func != CTORS_E; (*func++)()) ;

        for (auto func { CTORS_C }; func != CTORS_S; (*func++)()) ;

        // Now we're ready to talk to the world
        Console::print ("\nNOVA Microhypervisor #%07lx (%s): %s %s [%s]\n", reinterpret_cast<uintptr_t>(&GIT_VER), ARCH, __DATE__, __TIME__, COMPILER_STRING);
    }

    return 0;
}
