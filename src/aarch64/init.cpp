/*
 * Initialization Code
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

#include "acpi.hpp"
#include "buddy.hpp"
#include "console.hpp"
#include "extern.hpp"
#include "kmem.hpp"

extern "C"
void init (uintptr_t offset)
{
    if (!Acpi::resume) {

        Kmem::init (offset);

        Buddy::init();

        for (void (**func)() = &CTORS_S; func != &CTORS_E; (*func++)()) ;

        for (void (**func)() = &CTORS_C; func != &CTORS_S; (*func++)()) ;

        // Now we're ready to talk to the world
        Console::print ("\nNOVA Microhypervisor #%07lx (%s): %s %s [%s]\n", reinterpret_cast<uintptr_t>(&GIT_VER), ARCH, __DATE__, __TIME__, COMPILER_STRING);
    }
}
