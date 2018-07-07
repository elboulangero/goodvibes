/*
 * Libcaphe
 *
 * Copyright (C) 2016-2017 Arnaud Rebillout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBCAPHE_CAPHE_TRACE_H__
#define __LIBCAPHE_CAPHE_TRACE_H__

#include <glib.h>

#ifdef ENABLE_TRACE
#define CAPHE_TRACE_ENABLED 1
#else
#define CAPHE_TRACE_ENABLED 0
#endif

#define TRACE(fmt, ...) \
        do { \
                if (CAPHE_TRACE_ENABLED) \
                        g_debug("-> %s(" fmt ")", __func__, ##__VA_ARGS__); \
        } while (0)

#endif /* __LIBCAPHE_CAPHE_TRACE_H__ */
