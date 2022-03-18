/*
 * Paging: x86
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

#include "macros.hpp"

#ifdef __ASSEMBLER__

#define ATTR_P      BIT64  (0)          // Present
#define ATTR_W      BIT64  (1)          // Writable
#define ATTR_U      BIT64  (2)          // User
#define ATTR_A      BIT64  (5)          // Accessed
#define ATTR_D      BIT64  (6)          // Dirty
#define ATTR_S      BIT64  (7)          // Superpage
#define ATTR_G      BIT64  (8)          // Global

#else

class Paging
{
    public:
        enum Permissions
        {
            NONE    = 0,                // No Permissions
            R       = BIT  (0),         // Read
            W       = BIT  (1),         // Write
            XU      = BIT  (2),         // Execute (User)
            XS      = BIT  (3),         // Execute (Supervisor)
            API     = XS | XU | W | R,
            U       = BIT (12),         // User Accessible
            K       = BIT (13),         // Kernel Memory
            G       = BIT (14),         // Global
            SS      = BIT (15),         // Shadow Stack
        };
};

#endif
