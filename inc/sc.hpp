/*
 * Scheduling Context (SC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "atomic.hpp"
#include "compiler.hpp"
#include "kobject.hpp"
#include "queue.hpp"
#include "status.hpp"

class Ec;

class Sc final : public Kobject, public Queue<Sc>::Element
{
    friend class Scheduler;

    private:
        Ec *     const          ec                  { nullptr };
        uint64   const          budget              { 0 };
        unsigned const          cpu                 { 0 };
        uint16   const          cos                 { 0 };
        uint8    const          prio                { 0 };
        Atomic<uint64>          used                { 0 };
        uint64                  left                { 0 };
        uint64                  last                { 0 };

        static Slab_cache       cache;

        Sc (unsigned, Ec *, uint16, uint8, uint16);

    public:
        [[nodiscard]] static inline Sc *create (Status &s, unsigned n, Ec *e, uint16 b, uint8 p, uint16 c)
        {
            auto const sc { new (cache) Sc (n, e, b, p, c) };

            if (EXPECT_FALSE (!sc))
                s = Status::MEM_OBJ;

            return sc;
        }

        inline void destroy() { operator delete (this, cache); }

        ALWAYS_INLINE
        inline auto get_ec() const { return ec; }

        ALWAYS_INLINE
        inline uint64 get_used() const { return used; }
};

class Scheduler final
{
    public:
        static constexpr auto priorities { 128 };

        static void unblock (Sc *);
        static void requeue();

        ALWAYS_INLINE
        static inline auto get_current() { return current; }

        ALWAYS_INLINE
        static inline void set_current (Sc *s) { current = s; }

        [[noreturn]]
        static void schedule (bool = false);

    private:
        // Ready queue
        class Ready final
        {
            private:
                Queue<Sc>   queue[priorities];
                unsigned    prio_top { 0 };

            public:
                inline void enqueue (Sc *, uint64);
                inline auto dequeue (uint64);
        };

        // Release queue
        class Release final
        {
            private:
                Queue<Sc>   queue;
                Spinlock    lock;

            public:
                inline void enqueue (Sc *);
                inline auto dequeue();
        };

        static Ready        ready       CPULOCAL;
        static Release      release     CPULOCAL;
        static Sc *         current     CPULOCAL;
};
