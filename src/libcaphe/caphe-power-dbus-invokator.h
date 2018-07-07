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

#ifndef __LIBCAPHE_CAPHE_POWER_DBUS_INVOKATOR_H__
#define __LIBCAPHE_CAPHE_POWER_DBUS_INVOKATOR_H__

#include <glib-object.h>

#include "caphe-dbus-invokator.h"

/* GObject declarations */

#define CAPHE_TYPE_POWER_DBUS_INVOKATOR caphe_power_dbus_invokator_get_type()

G_DECLARE_FINAL_TYPE(CaphePowerDbusInvokator, caphe_power_dbus_invokator,
                     CAPHE, POWER_DBUS_INVOKATOR, CapheDbusInvokator)

/* Methods */

CapheDbusInvokator *caphe_power_dbus_invokator_new(void);

#endif /* __LIBCAPHE_CAPHE_POWER_DBUS_INVOKATOR_H__ */
