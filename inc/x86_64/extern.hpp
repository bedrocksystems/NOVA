/*
 * External Symbols
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

#include "types.hpp"

extern char PAGE_0;
extern char PAGE_1;
extern char PAGE_H;

extern char GIT_VER, NOVA_HPAS, KMEM_HVAS, KMEM_HVAF, PTAB_HVAS, DSTK_TOP;
extern void (*CTORS_S)(), (*CTORS_E)(), (*CTORS_C)(), (*CTORS_L)();

extern char entry_sys;
extern char entry_vmx;
extern uintptr_t handlers[];
extern mword hwdev_addr;
