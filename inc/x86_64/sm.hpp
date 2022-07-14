/*
 * Semaphore
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "ec.hpp"

class Sm : public Kobject, public Queue<Ec>
{
    private:
        mword counter;

        static Slab_cache cache;

    public:
        Sm (Pd *, mword, mword = 0);

        ALWAYS_INLINE
        inline void dn (bool zero, uint64 t)
        {
            Ec *ec = Ec::current;

            {   Lock_guard <Spinlock> guard (lock);

                if (counter) {
                    counter = zero ? 0 : counter - 1;
                    return;
                }

                enqueue (ec);
            }

            ec->set_timeout (t, this);

            ec->block_sc();
        }

        ALWAYS_INLINE
        inline void up()
        {
            Ec *ec;

            {   Lock_guard <Spinlock> guard (lock);

                if (!dequeue (ec = head())) {
                    counter++;
                    return;
                }
            }

            ec->release (Ec::sys_finish<Sys_regs::SUCCESS, true>);
        }

        ALWAYS_INLINE
        inline void timeout (Ec *ec)
        {
            {   Lock_guard <Spinlock> guard (lock);

                if (!dequeue (ec))
                    return;
            }

            ec->release (Ec::sys_finish<Sys_regs::COM_TIM>);
        }

        ALWAYS_INLINE
        static inline void *operator new (size_t) { return cache.alloc(); }

        ALWAYS_INLINE
        static inline void operator delete (void *ptr) { cache.free (ptr); }
};
