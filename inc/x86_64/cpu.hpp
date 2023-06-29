/*
 * Central Processing Unit (CPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "arch.hpp"
#include "compiler.hpp"
#include "config.hpp"
#include "extern.hpp"
#include "kmem.hpp"
#include "msr.hpp"
#include "selectors.hpp"
#include "spinlock.hpp"
#include "types.hpp"

class Cpu final
{
    private:
        static constexpr apic_t invalid_topology { BIT_RANGE (31, 0) };

        // Must agree with enum class Vendor below
        static constexpr char const *vendor_string[] { "Unknown", "GenuineIntel", "AuthenticAMD" };

        static constexpr struct scaleable_bus { uint8_t m, d; }
            freq_atom[BIT (4)] { { 5, 6 }, { 1, 1 }, { 4, 3 }, { 7, 6 }, { 4, 5 }, { 14, 15 }, { 9, 10 }, { 8, 9 }, { 7, 8 } },
            freq_core[BIT (3)] { { 8, 3 }, { 4, 3 }, { 2, 1 }, { 5, 3 }, { 10, 3 }, { 1, 1 }, { 4, 1 } };

        static void enumerate_clocks (uint32_t &, uint32_t &, scaleable_bus const *, unsigned);
        static void enumerate_clocks (uint32_t &, uint32_t &);

        static void enumerate_topology (uint32_t, uint32_t &, uint32_t (&)[4]);
        static void enumerate_features (uint32_t &, uint32_t &, uint32_t (&)[4], uint32_t (&)[12]);

        static void setup_msr();

        static inline Spinlock boot_lock    asm ("__boot_lock");

    public:
        enum class Vendor : unsigned
        {
            UNKNOWN,
            INTEL,
            AMD,
        };

        enum class Feature : unsigned
        {
            // 0x1.EDX
            MCE                     = 0 * 32 +  7,      // Machine Check Exception
            SEP                     = 0 * 32 + 11,      // SYSENTER/SYSEXIT Instructions
            MCA                     = 0 * 32 + 14,      // Machine Check Architecture
            ACPI                    = 0 * 32 + 22,      // Thermal Monitor and Software Controlled Clock Facilities
            HTT                     = 0 * 32 + 28,      // Hyper-Threading Technology
            // 0x1.ECX
            VMX                     = 1 * 32 +  5,      // Virtual Machine Extensions
            PCID                    = 1 * 32 + 17,      // Process Context Identifiers
            TSC_DEADLINE            = 1 * 32 + 24,      // TSC Deadline Support
            // 0x7.EBX
            SMEP                    = 3 * 32 +  7,      // Supervisor Mode Execution Prevention
            SMAP                    = 3 * 32 + 20,      // Supervisor Mode Access Prevention
            // 0x7.ECX
            UMIP                    = 4 * 32 +  2,      // User Mode Instruction Prevention
            // 0x7.EDX
            HYBRID                  = 5 * 32 + 15,      // Hybrid Processor
            // 0x80000001.EDX
            GB_PAGES                = 6 * 32 + 26,      // 1GB-Pages Support
            LM                      = 6 * 32 + 29,      // Long Mode Support
            // 0x80000001.ECX
            SVM                     = 7 * 32 +  2,
        };

        struct State_sys
        {
            uint64_t    star            { 0 };
            uint64_t    lstar           { 0 };
            uint64_t    fmask           { 0 };
            uint64_t    kernel_gs_base  { 0 };
        };

        static inline State_sys const hst_sys
        {
            .star  = static_cast<uint64_t>(SEL_KERN_DATA) << 48 | static_cast<uint64_t>(SEL_KERN_CODE) << 32,
            .lstar = reinterpret_cast<uintptr_t>(&entry_sys),
            .fmask = RFL_VIP | RFL_VIF | RFL_AC | RFL_VM | RFL_RF | RFL_NT | RFL_IOPL | RFL_DF | RFL_IF | RFL_TF,
            .kernel_gs_base = 0,
        };

        static cpu_t        id              CPULOCAL_HOT;
        static apic_t       topology        CPULOCAL_HOT;
        static unsigned     hazard          CPULOCAL_HOT;
        static Vendor       vendor          CPULOCAL;
        static unsigned     platform        CPULOCAL;
        static unsigned     family          CPULOCAL;
        static unsigned     model           CPULOCAL;
        static unsigned     stepping        CPULOCAL;
        static unsigned     patch           CPULOCAL;
        static uint32_t     features[8]     CPULOCAL;
        static bool         bsp             CPULOCAL;

        static inline cpu_t                 count  { 0 };

        static void init();

        static bool feature (Feature f)
        {
            return features[std::to_underlying (f) / 32] & BIT (std::to_underlying (f) % 32);
        }

        static void defeature (Feature f)
        {
            features[std::to_underlying (f) / 32] &= ~BIT (std::to_underlying (f) % 32);
        }

        static void preemption_disable()    { asm volatile ("cli" : : : "memory"); }
        static void preemption_enable()     { asm volatile ("sti" : : : "memory"); }
        static void preemption_point()      { asm volatile ("sti; nop; cli" : : : "memory"); }
        static void halt()                  { asm volatile ("sti; hlt; cli" : : : "memory"); }

        static void cpuid (unsigned leaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx)
        {
            asm volatile ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (leaf));
        }

        static void cpuid (unsigned leaf, unsigned subleaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx)
        {
            asm volatile ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (leaf), "c" (subleaf));
        }

        static auto remote_topology (cpu_t c) { return *Kmem::loc_to_glob (&topology, c); }

        static auto find_by_topology (uint32_t t)
        {
            for (cpu_t c { 0 }; c < count; c++)
                if (remote_topology (c) == t)
                    return c;

            return static_cast<cpu_t>(-1);
        }
};
