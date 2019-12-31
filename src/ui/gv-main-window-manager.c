/*
 * Goodvibes Radio Player
 *
 * Copyright (C) 2015-2018 Arnaud Rebillout
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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "framework/glib-object-additions.h"
#include "framework/gv-framework.h"
#include "ui/gv-ui-internal.h"
#include "ui/gv-main-window.h"

#include "ui/gv-main-window-manager.h"

/*
 * Reasons to have a separate object to deal with the main window configuration,
 * aka size, position, maximized.
 *
 * Delayed method calls
 * --------------------
 *
 * We invoke methods on the main window in two contexts:
 * - at init time (to load configuration)
 * - in signal handlers (to save configuration)
 *
 * Both of these contexts are not suitable to query the window size, and because
 * of that we need to delay all the method calls. This logic is better taken out
 * of the main window, and handled here in the window manager.
 *
 * Window finalization
 * -------------------
 *
 * Saving the window configuration is an operation that is delayed by a timeout,
 * and therefore there's a possibility that the main window is finalized before
 * the timeout is reached. If this 'save timeout' is handled by the main window,
 * then we have to try to save the window configuration from within the finalize
 * method of the window itself, and it doesn't work. It's too late to query the
 * window at this moment.
 *
 * The window manager solves this problem elegantly, since the 'save timeout' is
 * taken out of the main window, and handled by the manager. The manager is sure
 * to be finalized before the main window, and therefore can safely query it at
 * this moment.
 */

#define SAVE_DELAY 1 // how long to wait before writing changes to disk

/*
 * Properties
 */

enum {
	/* Reserved */
	PROP_0,
	/* Properties */
	PROP_MAIN_WINDOW,
	PROP_STATUS_ICON_MODE,
	PROP_AUTOSET_HEIGHT,
	/* Number of properties */
	PROP_N
};

static GParamSpec *properties[PROP_N];

/*
 * GObject definitions
 */

struct _GvMainWindowManagerPrivate {
	/* Properties */
	GvMainWindow *main_window;
	gboolean      status_icon_mode;
	gboolean      autoset_height;
	/* New values, waiting to be saved */
	gint new_x;
	gint new_y;
	gint new_width;
	gint new_height;
	/* Window configuration */
	guint save_window_configuration_source_id;
};

typedef struct _GvMainWindowManagerPrivate GvMainWindowManagerPrivate;

struct _GvMainWindowManager {
	/* Parent instance structure */
	GObject                     parent_instance;
	/* Private data */
	GvMainWindowManagerPrivate *priv;
};

static void gv_main_window_manager_configurable_interface_init(GvConfigurableInterface *iface);

G_DEFINE_TYPE_WITH_CODE(GvMainWindowManager, gv_main_window_manager, G_TYPE_OBJECT,
                        G_ADD_PRIVATE(GvMainWindowManager)
                        G_IMPLEMENT_INTERFACE(GV_TYPE_CONFIGURABLE,
                                        gv_main_window_manager_configurable_interface_init))

/*
 * Private methods
 */

static void
gv_main_window_manager_save_configuration_now(GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;
	GSettings *settings = gv_ui_settings;
	gint width, height;
	gint x, y;

	TRACE("%p", self);

	/* Should never be invoked in status icon mode */
	g_assert_false(priv->status_icon_mode);

	/* Save size if changed */
	g_settings_get(settings, "window-size", "(ii)", &width, &height);
	if ((width != priv->new_width) || (height != priv->new_height))
		g_settings_set(settings, "window-size", "(ii)", priv->new_width, priv->new_height);
	priv->new_width = priv->new_height = 0;

	/* Save position if changed */
	g_settings_get(settings, "window-position", "(ii)", &x, &y);
	if ((x != priv->new_x) || (y != priv->new_y))
		g_settings_set(settings, "window-position", "(ii)", priv->new_x, priv->new_y);
	priv->new_x = priv->new_y = 0;
}

static gboolean
when_timeout_save_window_configuration(GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	gv_main_window_manager_save_configuration_now(self);

	priv->save_window_configuration_source_id = 0;

	return G_SOURCE_REMOVE;
}

static void
gv_main_window_manager_save_configuration_delayed(GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	if (priv->save_window_configuration_source_id > 0)
		g_source_remove(priv->save_window_configuration_source_id);

	priv->save_window_configuration_source_id =
	        g_timeout_add_seconds(SAVE_DELAY, (GSourceFunc) when_timeout_save_window_configuration,
	                              self);
}

/*
 * Window signal handlers
 */

static void
on_main_window_notify_natural_height(GvMainWindow *window,
                                     GParamSpec *pspec G_GNUC_UNUSED,
                                     GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;
	gint natural_height;

	TRACE("%p, ..., %p", window, self);

	/* This signal handler is invoked when the natural height of the main
	 * window was changed, and therefore we want to resize it.
	 */

	/* Should never be invoked if autoset-height is FALSE */
	g_assert_true(priv->autoset_height);

	natural_height = gv_main_window_get_natural_height(window);
	gv_main_window_resize_height(window, natural_height);
}

static gboolean
on_main_window_configure_event(GtkWindow *window,
                               GdkEventConfigure *event,
                               GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	TRACE("%p, %d, %p", window, event->type, self);

	g_assert(event->type == GDK_CONFIGURE);

	/* Should never be invoked in status icon mode */
	g_assert_false(priv->status_icon_mode);

	/* Don't save anything when window is maximized */
	if (gtk_window_is_maximized(window))
		return GDK_EVENT_PROPAGATE;

	/* We can either use the values available in the GdkEventConfigure struct,
	 * or use GtkWindow getters. After a few tests (KDE/X11 and GNOME/Wayland),
	 * it turns out that the GdkEventConfigure values can't be used. For
	 * KDE/X11, the y value has an offset. For GNOME/Wayland, both x and y are
	 * zero, and both width and height are too big. So let's use GtkWindow
	 * getters.
	 */
	gtk_window_get_position(window, &priv->new_x, &priv->new_y);
	gtk_window_get_size(window, &priv->new_width, &priv->new_height);
	//DEBUG("GdkEventConfigure: x=%d, y=%d, widht=%d, height=%d",
	//		event->x, event->y, event->width, event->height);
	//DEBUG("GtkWindow getters: x=%d, y=%d, widht=%d, height=%d",
	//		priv->new_x, priv->new_y, priv->new_width, priv->new_height);

	/* Since this signal handler is invoked multiple times during resizing,
	 * we prefer to delay the actual save operation.
	 */
	gv_main_window_manager_save_configuration_delayed(self);

	return GDK_EVENT_PROPAGATE;
}

/*
 * Public functions
 */

GvMainWindowManager *
gv_main_window_manager_new(GvMainWindow *main_window, gboolean status_icon_mode)
{
	return g_object_new(GV_TYPE_MAIN_WINDOW_MANAGER,
	                    "main-window", main_window,
	                    "status-icon-mode", status_icon_mode,
	                    NULL);
}

/*
 * Property accessors
 */

static void
gv_main_window_manager_set_main_window(GvMainWindowManager *self, GvMainWindow *main_window)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	/* This is a construct-only property */
	g_assert_null(priv->main_window);
	g_assert_nonnull(main_window);
	g_set_object(&priv->main_window, main_window);
}

static void
gv_main_window_manager_set_status_icon_mode(GvMainWindowManager *self, gboolean status_icon_mode)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	/* This is a construct-only property */
	g_assert_false(priv->status_icon_mode);
	priv->status_icon_mode = status_icon_mode;
}

gboolean
gv_main_window_manager_get_autoset_height(GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	return priv->autoset_height;
}

void
gv_main_window_manager_set_autoset_height(GvMainWindowManager *self, gboolean autoset_height)
{
	GvMainWindowManagerPrivate *priv = self->priv;

	/* In status icon mode, this property is forced to TRUE */
	if (priv->status_icon_mode) {
		DEBUG("Status icon mode: 'autoset-height' forced to TRUE");
		autoset_height = TRUE;
	}

	if (priv->autoset_height == autoset_height)
		return;

	if (autoset_height) {
		g_signal_connect_object
		(priv->main_window, "notify::natural-height",
		 G_CALLBACK(on_main_window_notify_natural_height),
		 self, 0);
	} else {
		g_signal_handlers_disconnect_by_func
		(priv->main_window,
		 G_CALLBACK(on_main_window_notify_natural_height),
		 self);
	}

	priv->autoset_height = autoset_height;
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_AUTOSET_HEIGHT]);
}

static void
gv_main_window_manager_get_property(GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GvMainWindowManager *self = GV_MAIN_WINDOW_MANAGER(object);

	TRACE_GET_PROPERTY(object, property_id, value, pspec);

	switch (property_id) {
	case PROP_AUTOSET_HEIGHT:
		g_value_set_boolean(value, gv_main_window_manager_get_autoset_height(self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
gv_main_window_manager_set_property(GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GvMainWindowManager *self = GV_MAIN_WINDOW_MANAGER(object);

	TRACE_SET_PROPERTY(object, property_id, value, pspec);

	switch (property_id) {
	case PROP_MAIN_WINDOW:
		gv_main_window_manager_set_main_window(self, g_value_get_object(value));
		break;
	case PROP_STATUS_ICON_MODE:
		gv_main_window_manager_set_status_icon_mode(self, g_value_get_boolean(value));
		break;
	case PROP_AUTOSET_HEIGHT:
		gv_main_window_manager_set_autoset_height(self, g_value_get_boolean(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

/*
 * GvConfigurable interface
 */

static void
gv_main_window_manager_load_configuration(GvMainWindowManager *self)
{
	GvMainWindowManagerPrivate *priv = self->priv;
	GSettings *settings = gv_ui_settings;
	gint width, height;
	gint x, y;

	TRACE("%p", self);

	/* Get settings */
	g_settings_get(settings, "window-size", "(ii)", &width, &height);
	g_settings_get(settings, "window-position", "(ii)", &x, &y);
	g_settings_bind(settings, "window-autoset-height",
	                self, "autoset-height", G_SETTINGS_BIND_DEFAULT);

	/* Set initial window size */
	if (width != -1 && height != -1) {
		GtkWindow *window = GTK_WINDOW(priv->main_window);

		DEBUG("Restoring window size (%d, %d)", width, height);
		gtk_window_resize(window, width, height);

	} else {
		GtkWindow *window = GTK_WINDOW(priv->main_window);
		gint win_height;

		/*
		 * Now is the tricky part. I wish to resize the main window to its
		 * 'natural size', so that it can accomodate the station tree view
		 * (whatever the number of stations), without showing the scroll bar.
		 * I don't think I'm wishing anything fancy here.
		 *
		 * However, it seems impossible to find a reliable way to do that.
		 *
		 * Querying the station tree view for its natural size RIGHT NOW is
		 * too early. Ok, that's fine to me GTK, so when is the right moment ?
		 *
		 * Well, the problem is that there doesn't seem to be a right moment to
		 * speak of. I tried delaying to the latest, that's to say connecting to
		 * the 'map-event' signal of the station tree view, then delaying with a
		 * 'g_idle_add()', then at last querying the tree view for its natural size.
		 * Even at this moment, it might fail and report a size that is too small.
		 * After that I have no other signal to connect to, I'm left with delaying
		 * with a timeout to query AGAIN the tree view for its natural size, and
		 * discover, o surprise, that it reports a different natural size.
		 *
		 * And as far as I know, there was no signal that I could have used to
		 * get notified of this change.
		 *
		 * So, screw it, no more autosize, it's way too much hassle for such a
		 * trivial result.
		 */

		gtk_window_get_size(window, NULL, &win_height);
		win_height += 200;

		DEBUG("Setting default window size (%d, %d)", 1, win_height);
		gtk_window_resize(window, 1, win_height);
	}

	/* Set initial window position */
	if (x != -1 || y != -1) {
		GtkWindow *window = GTK_WINDOW(priv->main_window);

		DEBUG("Restoring window position (%d, %d)", x, y);
		gtk_window_move(window, x, y);
	}

	/* Connect to the 'configure-event' signal to save changes when the
	 * window size or position is modified.
	 */
	g_signal_connect_object(priv->main_window, "configure-event",
	                        G_CALLBACK(on_main_window_configure_event),
	                        self, 0);
}

static void
gv_main_window_manager_configure(GvConfigurable *configurable)
{
	GvMainWindowManager *self = GV_MAIN_WINDOW_MANAGER(configurable);
	GvMainWindowManagerPrivate *priv = self->priv;

	/* In status icon mode, configuration is ignored. We neither read nor write it.
	 * The only thing we want is to ensure that the height is automatically set.
	 * We don't even need to resize the window now, it will be done the first time
	 * the window is realized.
	 */
	if (priv->status_icon_mode) {
		gv_main_window_manager_set_autoset_height(self, TRUE);
		return;
	}

	/* Load configuration */
	gv_main_window_manager_load_configuration(self);
}

static void
gv_main_window_manager_configurable_interface_init(GvConfigurableInterface *iface)
{
	iface->configure = gv_main_window_manager_configure;
}

/*
 * GObject methods
 */

static void
gv_main_window_manager_finalize(GObject *object)
{
	GvMainWindowManager *self = GV_MAIN_WINDOW_MANAGER(object);
	GvMainWindowManagerPrivate *priv = self->priv;

	TRACE("%p", object);

	/* Run any pending save operation */
	if (priv->save_window_configuration_source_id > 0)
		when_timeout_save_window_configuration(self);

	/* Release resources */
	g_object_unref(priv->main_window);

	/* Chain up */
	G_OBJECT_CHAINUP_FINALIZE(gv_main_window_manager, object);
}

static void
gv_main_window_manager_constructed(GObject *object)
{
	GvMainWindowManager *self = GV_MAIN_WINDOW_MANAGER(object);
	GvMainWindowManagerPrivate *priv =self->priv;

	TRACE("%p", self);

	/* Ensure construct-only properties have been set */
	g_assert_nonnull(priv->main_window);

	/* Chain up */
	G_OBJECT_CHAINUP_CONSTRUCTED(gv_main_window_manager, object);
}

static void
gv_main_window_manager_init(GvMainWindowManager *self)
{
	TRACE("%p", self);

	/* Initialize private pointer */
	self->priv = gv_main_window_manager_get_instance_private(self);
}

static void
gv_main_window_manager_class_init(GvMainWindowManagerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);

	TRACE("%p", class);

	/* Override GObject methods */
	object_class->finalize = gv_main_window_manager_finalize;
	object_class->constructed = gv_main_window_manager_constructed;

	/* Properties */
	object_class->get_property = gv_main_window_manager_get_property;
	object_class->set_property = gv_main_window_manager_set_property;

	properties[PROP_MAIN_WINDOW] =
	        g_param_spec_object("main-window", "Main window", NULL,
	                            GV_TYPE_MAIN_WINDOW,
	                            GV_PARAM_DEFAULT_FLAGS | G_PARAM_CONSTRUCT_ONLY |
	                            G_PARAM_WRITABLE);

	properties[PROP_STATUS_ICON_MODE] =
	        g_param_spec_boolean("status-icon-mode", "Status icon mode", NULL,
	                             FALSE,
	                             GV_PARAM_DEFAULT_FLAGS | G_PARAM_CONSTRUCT_ONLY |
	                             G_PARAM_WRITABLE);

	properties[PROP_AUTOSET_HEIGHT] =
	        g_param_spec_boolean("autoset-height", "Autoset height", NULL,
	                             FALSE,
	                             GV_PARAM_DEFAULT_FLAGS | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, PROP_N, properties);
}
