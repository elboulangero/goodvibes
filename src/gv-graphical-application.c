/*
 * Goodvibes Radio Player
 *
 * Copyright (C) 2017-2018 Arnaud Rebillout
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

#include "framework/gv-framework.h"
#include "core/gv-core.h"
#include "ui/gv-ui.h"
#include "feat/gv-feat.h"

#include "gv-graphical-application.h"
#include "options.h"

#define APPMENU_RESOURCE_PATH GV_APPLICATION_PATH "/Ui/app-menu.glade"
#define MENUBAR_RESOURCE_PATH GV_APPLICATION_PATH "/Ui/menubar.glade"

/*
 * GObject definitions
 */

struct _GvGraphicalApplication {
	/* Parent instance structure */
	GtkApplication parent_instance;
};

G_DEFINE_TYPE(GvGraphicalApplication, gv_graphical_application, GTK_TYPE_APPLICATION)

/*
 * Public methods
 */

GApplication *
gv_graphical_application_new(const gchar *application_id)
{
	return G_APPLICATION(g_object_new(GV_TYPE_GRAPHICAL_APPLICATION,
	                                  "application-id", application_id,
	                                  "flags", G_APPLICATION_FLAGS_NONE,
	                                  NULL));
}

/*
 * GApplication actions
 */

static void
add_station_action_cb(GSimpleAction *action G_GNUC_UNUSED,
                      GVariant      *parameters G_GNUC_UNUSED,
                      gpointer       user_data G_GNUC_UNUSED)
{
	gv_ui_present_add_station();
}

static void
preferences_action_cb(GSimpleAction *action G_GNUC_UNUSED,
                      GVariant      *parameters G_GNUC_UNUSED,
                      gpointer       user_data G_GNUC_UNUSED)
{
	gv_ui_present_preferences();
}

static void
help_action_cb(GSimpleAction *action G_GNUC_UNUSED,
               GVariant      *parameters G_GNUC_UNUSED,
               gpointer       user_data G_GNUC_UNUSED)
{
	g_app_info_launch_default_for_uri(GV_ONLINE_HELP, NULL, NULL);
}

static void
about_action_cb(GSimpleAction *action G_GNUC_UNUSED,
                GVariant      *parameters G_GNUC_UNUSED,
                gpointer       user_data G_GNUC_UNUSED)
{
	gv_ui_present_about();
}

static void
close_ui_action_cb(GSimpleAction *action G_GNUC_UNUSED,
                   GVariant      *parameters G_GNUC_UNUSED,
                   gpointer       user_data G_GNUC_UNUSED)
{
	gv_ui_hide();
}

static void
quit_action_cb(GSimpleAction *action G_GNUC_UNUSED,
               GVariant      *parameters G_GNUC_UNUSED,
               gpointer       user_data G_GNUC_UNUSED)
{
	gv_core_quit();
}

static const GActionEntry gv_graphical_application_actions[] = {
	{ "add-station", add_station_action_cb, NULL, NULL, NULL, {0} },
	{ "preferences", preferences_action_cb, NULL, NULL, NULL, {0} },
	{ "help",        help_action_cb,        NULL, NULL, NULL, {0} },
	{ "about",       about_action_cb,       NULL, NULL, NULL, {0} },
	{ "close-ui",    close_ui_action_cb,    NULL, NULL, NULL, {0} },
	{ "quit",        quit_action_cb,        NULL, NULL, NULL, {0} },
	{ NULL,          NULL,                  NULL, NULL, NULL, {0} }
};

/*
 * GApplication methods
 */

static void
gv_graphical_application_shutdown(GApplication *app)
{
	DEBUG_NO_CONTEXT(">>>> Main loop terminated <<<<");

	/* Cleanup */
	DEBUG_NO_CONTEXT("---- Cleaning up ----");
	gv_feat_cleanup();
	gv_ui_cleanup();
	gv_core_cleanup();
	gv_framework_cleanup();

	/* Mandatory chain-up */
	G_APPLICATION_CLASS(gv_graphical_application_parent_class)->shutdown(app);
}

static void
gv_graphical_application_startup(GApplication *app)
{
	DEBUG_NO_CONTEXT("---- Starting application ----");

	/* Mandatory chain-up, see:
	 * https://developer.gnome.org/gtk3/stable/GtkApplication.html#gtk-application-new
	 */
	G_APPLICATION_CLASS(gv_graphical_application_parent_class)->startup(app);

	/* Add actions to the application */
	g_action_map_add_action_entries(G_ACTION_MAP(app),
	                                gv_graphical_application_actions,
	                                -1,
	                                NULL);

	/* The 'close-ui' action makes no sense in the status icon mode */
	if (options.status_icon)
		g_action_map_remove_action(G_ACTION_MAP(app), "close-ui");

	/* Now time to setup the menus. In status icon mode, we do nothing, this is handled
	 * by the status icon code itself later on.
	 */
	if (!options.status_icon) {
		gboolean prefers_app_menu;

		/* Check how the application prefers to display it's main menu */
		prefers_app_menu = gtk_application_prefers_app_menu(GTK_APPLICATION(app));
		DEBUG("Application prefers... %s", prefers_app_menu ? "app-menu" : "menubar");

		/* Gnome-based desktop environments prefer an application menu */
		if (prefers_app_menu) {
			GtkBuilder *builder;
			GMenuModel *model;

			builder = gtk_builder_new_from_resource(APPMENU_RESOURCE_PATH);
			model = G_MENU_MODEL(gtk_builder_get_object(builder, "app-menu"));
			gtk_application_set_app_menu(GTK_APPLICATION(app), model);
			g_object_unref(builder);
		}

		/* Unity-based and traditional desktop environments prefer a menu bar */
		if (!prefers_app_menu) {
			GtkBuilder *builder;
			GMenuModel *model;

			builder = gtk_builder_new_from_resource(MENUBAR_RESOURCE_PATH);
			model = G_MENU_MODEL(gtk_builder_get_object(builder, "menubar"));
			gtk_application_set_menubar(GTK_APPLICATION(app), model);
			g_object_unref(builder);
		}
	}

	/* Initialization */
	DEBUG_NO_CONTEXT("---- Initializing ----");
	gv_framework_init();
	gv_core_init(app);
	gv_ui_init(app, options.status_icon);
	gv_feat_init();
	gv_framework_init_completed();

	/* Configuration */
	DEBUG_NO_CONTEXT("---- Configuring ----");
	gv_feat_configure_early();
	gv_core_configure();
	gv_ui_configure();
	gv_feat_configure_late();

	/* Hold application */
	g_application_hold(app);
}

static gboolean
when_idle_go_player(gpointer user_data)
{
	const gchar *uri_to_play = user_data;

	gv_player_go(gv_core_player, uri_to_play);

	return G_SOURCE_REMOVE;
}

static void
gv_graphical_application_activate(GApplication *app G_GNUC_UNUSED)
{
	static gboolean first_invocation = TRUE;

	if (first_invocation) {
		first_invocation = FALSE;

		DEBUG_NO_CONTEXT(">>>> Main loop started <<<<");

		/* Schedule a callback to play music.
		 * DO NOT start playing now ! It's too early !
		 * There's still some init code pending, and we want to ensure
		 * (as much as possible) that this init code is run before we
		 * start the playback. Therefore we schedule with a low priority.
		 */
		g_idle_add_full(G_PRIORITY_LOW, when_idle_go_player,
		                (void *) options.uri_to_play, NULL);

		/* Present the main window, depending on options */
		if (!options.without_ui) {
			DEBUG("Presenting main window");
			gv_ui_present_main();
		} else {
			DEBUG("NOT presenting main window (--without-ui)");
		}

	} else {
		/* Present the main window, unconditionally */
		DEBUG("Presenting main window");
		gv_ui_present_main();
	}
}

/*
 * GObject methods
 */

static void
gv_graphical_application_init(GvGraphicalApplication *self)
{
	TRACE("%p", self);
}

static void
gv_graphical_application_class_init(GvGraphicalApplicationClass *class)
{
	GApplicationClass *application_class = G_APPLICATION_CLASS(class);

	TRACE("%p", class);

	/* Override GApplication methods */
	application_class->startup  = gv_graphical_application_startup;
	application_class->shutdown = gv_graphical_application_shutdown;
	application_class->activate = gv_graphical_application_activate;
}
