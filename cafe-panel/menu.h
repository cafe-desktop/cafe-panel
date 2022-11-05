/*
 * Copyright (C) 1997 - 2000 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2004 Red Hat Inc.
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
 */

#ifndef __MENU_H__
#define __MENU_H__

#include "panel-widget.h"
#include "applet.h"
#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

void		setup_menuitem		  (CtkWidget        *menuitem,
					   CtkIconSize       icon_size,
					   CtkWidget        *pixmap,
					   const char       *title);
void            setup_menuitem_with_icon (CtkWidget         *menuitem,
					  CtkIconSize       icon_size,
					  GIcon             *gicon,
					  const char        *image_filename,
					  const char        *title);

CtkWidget      *create_empty_menu         (void);
CtkWidget      *create_applications_menu  (const char  *menu_file,
					   const char  *menu_path,
					   gboolean    always_show_image);
CtkWidget      *create_main_menu          (PanelWidget *panel);

void		setup_internal_applet_drag (CtkWidget             *menuitem,
					    PanelActionButtonType  type);
void            setup_uri_drag             (CtkWidget  *menuitem,
					    const char *uri,
					    const char *icon,
						GdkDragAction action);

CtkWidget *	panel_create_menu              (void);

CtkWidget *	panel_image_menu_item_new      (void);

GdkPixbuf *	panel_make_menu_icon (CtkIconTheme *icon_theme,
				      const char   *icon,
				      const char   *fallback,
				      int           size,
				      gboolean     *long_operation);

GdkScreen      *menuitem_to_screen   (CtkWidget *menuitem);
PanelWidget    *menu_get_panel       (CtkWidget *menu);
CtkWidget      *add_menu_separator   (CtkWidget *menu);

gboolean menu_dummy_button_press_event (CtkWidget      *menuitem,
					GdkEventButton *event);


#ifdef __cplusplus
}
#endif

#endif /* __MENU_H__ */
