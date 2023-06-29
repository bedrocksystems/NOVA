/*
 * Instruction Patching
 *
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

#include "lapic.hpp"
#include "patch.hpp"
#include "string.hpp"

void Patch::detect()
{
    uint32_t eax, ebx, ecx, edx;

    Cpu::cpuid (0x0, eax, ebx, ecx, edx);

    switch (static_cast<uint8_t>(eax)) {
        default:
            Cpu::cpuid (0x1, 0x0, eax, ebx, ecx, edx);
            Lapic::x2apic = ecx & BIT (21);
            [[fallthrough]];
        case 0x0:
            break;
    }

    Cpu::cpuid (0x80000000, eax, ebx, ecx, edx);

    switch (static_cast<uint8_t>(eax)) {
        default:
            break;
    }
}

void Patch::init()
{
    extern Patch PATCH_S, PATCH_E;

    for (auto p { &PATCH_S }; p < &PATCH_E; p++) {

        if (skipped & BIT (p->tag))
            continue;

        auto const o { reinterpret_cast<uint8_t *>(p) + p->off_old };
        auto const n { reinterpret_cast<uint8_t *>(p) + p->off_new };

        memcpy (o, n, p->len_new);
        memset (o + p->len_new, NOP_OPC, p->len_old - p->len_new);
    }
}
