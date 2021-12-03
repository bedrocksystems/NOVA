/*
 * UART Console (PIO)
 *
 * Copyright (C) 2019-2020 Udo Steinberg, BedRock Systems, Inc.
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

#include "console_uart.hpp"

class Console_uart_pio : protected Console_uart
{
    protected:
        uint16 port_base { 0 };

        void pmap (uint16 p)
        {
            if (!p || port_base)
                return;

            port_base = p;

            init();

            enable();
        }

        inline Console_uart_pio (unsigned c) : Console_uart (c) {}
};
