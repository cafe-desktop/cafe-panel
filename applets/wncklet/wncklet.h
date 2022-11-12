/* vncklet.h
 *
 * Copyright (C) 2003  Wipro Technologies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Arvind Samptur <arvind.samptur@wipro.com>
 *
 */

#ifndef __VNCKLET_H__
#define __VNCKLET_H__

#include <libvnck/libvnck.h>

#include <glib.h>
#include <ctk/ctk.h>
#include <cafe-panel-applet.h>

#define VNCKLET_RESOURCE_PATH "/org/cafe/panel/applet/vncklet/"

#ifdef __cplusplus
extern "C" {
#endif

void vncklet_display_help(CtkWidget* widget, const char* doc_id, const char* link_id, const char* icon_name);

VnckScreen* vncklet_get_screen(CtkWidget* applet);

void vncklet_connect_while_alive(gpointer object, const char* signal, GCallback func, gpointer func_data, gpointer alive_object);

#ifdef __cplusplus
}
#endif

#endif
