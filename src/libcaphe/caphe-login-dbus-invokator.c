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

#include <stdlib.h>

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "caphe-trace.h"
#include "caphe-dbus-invokator.h"
#include "caphe-login-dbus-invokator.h"

/*
 * Debug macros
 */

#define debug(proxy, fmt, ...)    \
        g_debug("[%s]: " fmt, \
                proxy ? g_dbus_proxy_get_name(proxy) : "SessionManager", \
                ##__VA_ARGS__)

/*
 * GObject definitions
 */

struct _CapheLoginDbusInvokatorPrivate {
	gint32 fd;
};

typedef struct _CapheLoginDbusInvokatorPrivate CapheLoginDbusInvokatorPrivate;

struct _CapheLoginDbusInvokator {
	/* Parent instance structure */
	CapheDbusInvokator              parent_instance;
	/* Private data */
	CapheLoginDbusInvokatorPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE(CapheLoginDbusInvokator, caphe_login_dbus_invokator,
                           CAPHE_TYPE_DBUS_INVOKATOR)

/*
 * DbusInvokator methods
 */

static gboolean
caphe_login_dbus_invokator_is_inhibited(CapheDbusInvokator *dbus_invokator)
{
	CapheLoginDbusInvokator *self = CAPHE_LOGIN_DBUS_INVOKATOR(dbus_invokator);
	CapheLoginDbusInvokatorPrivate *priv = self->priv;

	return priv->fd != -1;
}

static void
caphe_login_dbus_invokator_uninhibit(CapheDbusInvokator *dbus_invokator)
{
	CapheLoginDbusInvokator *self = CAPHE_LOGIN_DBUS_INVOKATOR(dbus_invokator);
	CapheLoginDbusInvokatorPrivate *priv = self->priv;
	GDBusProxy *proxy = caphe_dbus_invokator_get_proxy(dbus_invokator);

	if (priv->fd < 0) {
		debug(proxy, "Already uninhibited (no fd)");
		return;
	}

	close(priv->fd);
	priv->fd = -1;

	debug(proxy, "Uninhibited");
}

static gboolean
caphe_login_dbus_invokator_inhibit(CapheDbusInvokator *dbus_invokator,
                                   const gchar *application, const gchar *reason,
                                   GError **error)
{
	CapheLoginDbusInvokator *self = CAPHE_LOGIN_DBUS_INVOKATOR(dbus_invokator);
	CapheLoginDbusInvokatorPrivate *priv = self->priv;
	GDBusProxy *proxy = caphe_dbus_invokator_get_proxy(dbus_invokator);
	GUnixFDList *fd_list = NULL;
	gint32 fd_index;
	GVariant *res;

	if (priv->fd >= 0) {
		debug(proxy, "Already inhibited (fd: %d)", priv->fd);
		return TRUE;
	}

	if (proxy == NULL) {
		debug(proxy, "No proxy");
		return FALSE;
	}

	/*
	 * Documentation at:
	 * https://www.freedesktop.org/wiki/Software/systemd/inhibit/
	 */
	res = g_dbus_proxy_call_with_unix_fd_list_sync(proxy,
	                "Inhibit",
	                g_variant_new(
	                        "(ssss)",
	                        "sleep",
	                        application,
	                        reason,
	                        "block"),
	                G_DBUS_CALL_FLAGS_NONE,
	                -1,
	                NULL, /* in_fd_list */
	                &fd_list, /* out_fd_list */
	                NULL, /* cancellable */
	                error);

	if (res == NULL)
		return FALSE;

	g_variant_get(res, "(h)", &fd_index);
	g_variant_unref(res);
	debug(proxy, "Fd index: %d", fd_index);

	priv->fd = g_unix_fd_list_get(fd_list, fd_index, NULL);
	g_object_unref(fd_list);
	debug(proxy, "Inhibited (fd: %d)", priv->fd);

	return TRUE;
}

/*
 * GObject methods
 */

static void
caphe_login_dbus_invokator_finalize(GObject *object)
{
	CapheDbusInvokator *dbus_invokator = CAPHE_DBUS_INVOKATOR(object);

	TRACE("%p", object);

	/* Ensure uninhibition */
	caphe_login_dbus_invokator_uninhibit(dbus_invokator);

	if (G_OBJECT_CLASS(caphe_login_dbus_invokator_parent_class)->finalize)
		G_OBJECT_CLASS(caphe_login_dbus_invokator_parent_class)->finalize(object);
}

static void
caphe_login_dbus_invokator_init(CapheLoginDbusInvokator *self)
{
	CapheLoginDbusInvokatorPrivate *priv =
	        caphe_login_dbus_invokator_get_instance_private(self);

	TRACE("%p", self);

	/* Initialize inhibit file descriptor */
	priv->fd = -1;

	/* Initialize private pointer */
	self->priv = priv;
}

static void
caphe_login_dbus_invokator_class_init(CapheLoginDbusInvokatorClass *class)
{
	GObjectClass            *object_class = G_OBJECT_CLASS(class);
	CapheDbusInvokatorClass *dbus_invokator_class = CAPHE_DBUS_INVOKATOR_CLASS(class);

	TRACE("%p", class);

	/* GObject methods */
	object_class->finalize             = caphe_login_dbus_invokator_finalize;

	/* Implement methods */
	dbus_invokator_class->inhibit      = caphe_login_dbus_invokator_inhibit;
	dbus_invokator_class->uninhibit    = caphe_login_dbus_invokator_uninhibit;
	dbus_invokator_class->is_inhibited = caphe_login_dbus_invokator_is_inhibited;
}
