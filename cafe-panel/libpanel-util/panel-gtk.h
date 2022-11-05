/*
 * panel-ctk.h: various small extensions to ctk+
 *
 * Copyright (C) 2009-2010 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_CTK_H
#define PANEL_CTK_H

#include <ctk/ctk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_CTK_BUILDER_GET(builder, name) CTK_WIDGET (ctk_builder_get_object (builder, name))

void panel_ctk_file_chooser_add_image_preview (CtkFileChooser *chooser);

CtkWidget* panel_dialog_add_button (CtkDialog   *dialog,
				    const gchar *button_text,
				    const gchar *icon_name,
				          gint   response_id);

CtkWidget* panel_file_chooser_dialog_new (const gchar          *title,
					  CtkWindow            *parent,
					  CtkFileChooserAction  action,
					  const gchar          *first_button_text,
					  ...);

CtkWidget* panel_image_menu_item_new_from_icon (const gchar *icon_name,
						const gchar *label_name);

CtkWidget* panel_image_menu_item_new_from_gicon (GIcon       *gicon,
						 const gchar *label_name);

CtkWidget* panel_check_menu_item_new (CtkWidget *widget_check);

#ifdef __cplusplus
}
#endif

#endif /* PANEL_CTK_H */
