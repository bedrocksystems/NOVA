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
#include "atomic.hpp"
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
        static void setup_pstate();

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
            PAT                     = 0 * 32 + 16,      // Page Attribute Table
            ACPI                    = 0 * 32 + 22,      // Thermal Monitor and Software Controlled Clock Facilities
            HTT                     = 0 * 32 + 28,      // Hyper-Threading Technology
            // 0x1.ECX
            VMX                     = 1 * 32 +  5,      // Virtual Machine Extensions
            EIST                    = 1 * 32 +  7,      // Enhanced Intel SpeedStep Technology
            PCID                    = 1 * 32 + 17,      // Process Context Identifiers
            X2APIC                  = 1 * 32 + 21,      // x2APIC Support
            TSC_DEADLINE            = 1 * 32 + 24,      // TSC Deadline Support
            XSAVE                   = 1 * 32 + 26,      // XCR0, XSETBV/XGETBV/XSAVE/XRSTOR Instructions
            RDRAND                  = 1 * 32 + 30,      // RDRAND Instruction
            // 0x6.EAX
            TURBO_BOOST             = 2 * 32 +  1,      // Turbo Boost Technology
            ARAT                    = 2 * 32 +  2,      // Always Running APIC Timer
            HWP                     = 2 * 32 +  7,      // HWP Baseline Resource and Capability
            HWP_NTF                 = 2 * 32 +  8,      // HWP Notification
            HWP_ACT                 = 2 * 32 +  9,      // HWP Activity Window
            HWP_EPP                 = 2 * 32 + 10,      // HWP Energy Performance Preference
            HWP_PLR                 = 2 * 32 + 11,      // HWP Package Level Request
            HWP_CAP                 = 2 * 32 + 15,      // HWP Capabilities
            HWP_PECI                = 2 * 32 + 16,      // HWP PECI Override
            HWP_FLEX                = 2 * 32 + 17,      // HWP Flexible
            HWP_FAM                 = 2 * 32 + 18,      // HWP Fast Access Mode
            // 0x7.EBX
            SMEP                    = 3 * 32 +  7,      // Supervisor Mode Execution Prevention
            RDT_M                   = 3 * 32 + 12,      // RDT Monitoring (PQM)
            RDT_A                   = 3 * 32 + 15,      // RDT Allocation (PQE)
            RDSEED                  = 3 * 32 + 18,      // RDSEED Instruction
            SMAP                    = 3 * 32 + 20,      // Supervisor Mode Access Prevention
            // 0x7.ECX
            UMIP                    = 4 * 32 +  2,      // User Mode Instruction Prevention
            CET_SS                  = 4 * 32 +  7,      // CET Shadow Stack
            TME                     = 4 * 32 + 13,      // Total Memory Encryption
            RDPID                   = 4 * 32 + 22,      // RDPID Instruction
            // 0x7.EDX
            HYBRID                  = 5 * 32 + 15,      // Hybrid Processor
            PCONFIG                 = 5 * 32 + 18,      // PCONFIG Instruction
            CET_IBT                 = 5 * 32 + 20,      // CET Indirect Branch Tracking
            // 0x80000001.EDX
            GB_PAGES                = 6 * 32 + 26,      // 1GB-Pages Support
            RDTSCP                  = 6 * 32 + 27,      // RDTSCP Instruction
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

            /*
             * Switch SYSCALL state between guest/host
             *
             * VMM-provided guest state was sanitized by constrain_* functions below
             *
             * @param o     Old live state
             * @param n     New live state
             */
            ALWAYS_INLINE
            static inline void make_current (State_sys const &o, State_sys const &n)
            {
                if (EXPECT_FALSE (o.star != n.star))
                    Msr::write (Msr::Register::IA32_STAR, n.star);

                if (EXPECT_FALSE (o.lstar != n.lstar))
                    Msr::write (Msr::Register::IA32_LSTAR, n.lstar);

                if (EXPECT_FALSE (o.fmask != n.fmask))
                    Msr::write (Msr::Register::IA32_FMASK, n.fmask);

                if (EXPECT_FALSE (o.kernel_gs_base != n.kernel_gs_base))
                    Msr::write (Msr::Register::IA32_KERNEL_GS_BASE, n.kernel_gs_base);
            }

            /*
             * Canonicalize virtual address to ensure WRMSR does not fault
             *
             * @param v     Virtual address provided by VMM
             * @return      Canonicalized virtual address
             */
            ALWAYS_INLINE
            static inline uint64_t constrain_canon (uint64_t v)
            {
                static constexpr auto va_bits { 48 };

                return static_cast<int64_t>(v) << (64 - va_bits) >> (64 - va_bits);
            }

            /*
             * Constrain STAR value to ensure WRMSR does not fault
             *
             * @param v     STAR value provided by VMM
             * @return      Constrained value
             */
            ALWAYS_INLINE
            static inline uint64_t constrain_star (uint64_t v)
            {
                return v & BIT64_RANGE (63, 32);
            }

            /*
             * Constrain FMASK value to ensure WRMSR does not fault
             *
             * @param v     FMASK value provided by VMM
             * @return      Constrained value
             */
            ALWAYS_INLINE
            static inline uint64_t constrain_fmask (uint64_t v)
            {
                return v & BIT64_RANGE (31, 0);
            }
        };

        struct State_tsc
        {
            uint64_t    tsc_aux         { 0 };

            /*
             * Switch TSC state between guest/host
             *
             * VMM-provided guest state was sanitized by constrain_* functions below
             *
             * @param o     Old live state
             * @param n     New live state
             */
            ALWAYS_INLINE
            static inline void make_current (State_tsc const &o, State_tsc const &n)
            {
                if (EXPECT_FALSE (o.tsc_aux != n.tsc_aux))
                    Msr::write (Msr::Register::IA32_TSC_AUX, n.tsc_aux);
            }

            /*
             * Constrain TSC_AUX value to ensure WRMSR does not fault
             *
             * @param v     TSC_AUX value provided by VMM
             * @return      Constrained value
             */
            ALWAYS_INLINE
            static inline uint64_t constrain_tsc_aux (uint64_t v)
            {
                return EXPECT_TRUE (feature (Feature::RDPID) || feature (Feature::RDTSCP)) ? v & BIT64_RANGE (31, 0) : 0;
            }
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
        static State_tsc    hst_tsc         CPULOCAL;

        static inline cpu_t                 count  { 0 };
        static inline Atomic<cpu_t>         online { 0 };

        static void init();
        static void fini();

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
