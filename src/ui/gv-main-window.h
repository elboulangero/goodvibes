/*
 * Goodvibes Radio Player
 *
 * Copyright (C) 2015-2017 Arnaud Rebillout
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

#ifndef __GOODVIBES_UI_GV_MAIN_WINDOW_H__
#define __GOODVIBES_UI_GV_MAIN_WINDOW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

/* GObject declarations */

#define GV_TYPE_MAIN_WINDOW gv_main_window_get_type()

G_DECLARE_FINAL_TYPE(GvMainWindow, gv_main_window, GV, MAIN_WINDOW, GtkApplicationWindow)

/* Methods */

GtkWidget *gv_main_window_new(GApplication *application, gboolean status_icon_mode);

void gv_main_window_resize_height(GvMainWindow *self, gint height);

/* Property accessors */

gint     gv_main_window_get_natural_height    (GvMainWindow *self);
gboolean gv_main_window_get_prefer_dark_theme (GvMainWindow *self);
void     gv_main_window_set_prefer_dark_theme (GvMainWindow *self, gboolean dark_theme);

#endif /* __GOODVIBES_UI_GV_MAIN_WINDOW_H__ */
