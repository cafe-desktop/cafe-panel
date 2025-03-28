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

#include <config.h>

#include "menu.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>
#include <libcafe-desktop/cafe-gsettings.h>
#include <cafemenu-tree.h>

#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-xdg.h>

#include "launcher.h"
#include "panel-util.h"
#include "panel.h"
#include "drawer.h"
#include "panel-config-global.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-profile.h"
#include "panel-menu-button.h"
#include "panel-menu-items.h"
#include "panel-globals.h"
#include "panel-run-dialog.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

static CtkWidget *populate_menu_from_directory (CtkWidget          *menu,
						CafeMenuTreeDirectory *directory);

static gboolean panel_menu_key_press_handler (CtkWidget   *widget,
					      CdkEventKey *event);

static inline gboolean desktop_is_home_dir(void)
{
	gboolean retval = FALSE;

	if (cafe_gsettings_schema_exists (BAUL_PREFS_SCHEMA)) {
		GSettings *settings;

		settings = g_settings_new (BAUL_PREFS_SCHEMA);
		retval = g_settings_get_boolean (settings, BAUL_PREFS_DESKTOP_IS_HOME_DIR_KEY);
		g_object_unref (settings);
	}

	return retval;
}

CtkWidget *
add_menu_separator (CtkWidget *menu)
{
	CtkWidget *menuitem;

	menuitem = ctk_separator_menu_item_new ();
	ctk_widget_set_sensitive (menuitem, FALSE);
	ctk_widget_show (menuitem);
	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);

	return menuitem;
}

static void
activate_app_def (CtkWidget      *menuitem,
		  CafeMenuTreeEntry *entry)
{
	const char       *path;

	path = cafemenu_tree_entry_get_desktop_file_path (entry);
	panel_menu_item_activate_desktop_file (menuitem, path);
}

PanelWidget *
menu_get_panel (CtkWidget *menu)
{
	PanelWidget *retval = NULL;

	g_return_val_if_fail (menu != NULL, NULL);

	if (CTK_IS_MENU_ITEM (menu))
		menu = ctk_widget_get_parent (menu);

	g_return_val_if_fail (CTK_IS_MENU (menu), NULL);

	while (menu) {
		retval = g_object_get_data (G_OBJECT (menu), "menu_panel");
		if (retval)
			break;

		menu = ctk_widget_get_parent (ctk_menu_get_attach_widget (CTK_MENU (menu)));
		if (!CTK_IS_MENU (menu))
			break;
	}

	if (retval && !PANEL_IS_WIDGET (retval)) {
		g_warning ("Invalid PanelWidget associated with menu");
		retval = NULL;
	}

	if (!retval) {
		g_warning ("Cannot find the PanelWidget associated with menu");
		retval = panels->data;
	}

	return retval;
}

static void
setup_menu_panel (CtkWidget *menu)
{
	PanelWidget *panel;

	panel = g_object_get_data (G_OBJECT (menu), "menu_panel");
	if (panel)
		return;

	panel = menu_get_panel (menu);
	g_object_set_data (G_OBJECT (menu), "menu_panel", panel);

	if (panel)
		ctk_menu_set_screen (CTK_MENU (menu),
				     ctk_widget_get_screen (CTK_WIDGET (panel)));
}

CdkScreen *
menuitem_to_screen (CtkWidget *menuitem)
{
	PanelWidget *panel_widget;

	panel_widget = menu_get_panel (menuitem);

	return ctk_window_get_screen (CTK_WINDOW (panel_widget->toplevel));
}

CtkWidget *
panel_create_menu (void)
{
	CtkWidget       *retval;

	retval = ctk_menu_new ();

	ctk_menu_set_reserve_toggle_size (CTK_MENU (retval), TRUE);

	ctk_widget_set_name (retval, "cafe-panel-main-menu");

	g_signal_connect (retval, "key_press_event",
			  G_CALLBACK (panel_menu_key_press_handler),
			  NULL);

	return retval;
}

CtkWidget *
create_empty_menu (void)
{
	CtkWidget *retval;

	retval = panel_create_menu ();

	g_signal_connect (retval, "show", G_CALLBACK (setup_menu_panel), NULL);

	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (retval, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return retval;
}

static void
add_app_to_panel (CtkWidget      *item,
		  CafeMenuTreeEntry *entry)
{
	PanelWidget   *panel_widget;
	PanelToplevel *toplevel;
	PanelData     *pd;
	int            position;

	panel_widget = menu_get_panel (item);
	toplevel = panel_widget->toplevel;

	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");
	position = pd ?  pd->insertion_pos : -1;

	panel_launcher_create (toplevel,
			       position,
			       cafemenu_tree_entry_get_desktop_file_path (entry));
}


static void
add_app_to_desktop (CtkWidget         *item G_GNUC_UNUSED,
		    CafeMenuTreeEntry *entry)
{
	char       *source_uri;
	const char *source;
	char       *target_dir;
	char       *target_uri;
	char       *target;
	GError     *error;

	g_return_if_fail (entry != NULL);

	if (desktop_is_home_dir ()) {
		target_dir = g_build_filename (g_get_home_dir (), NULL);
	} else {
		target_dir = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
	}

	source = cafemenu_tree_entry_get_desktop_file_path (entry);
	source_uri = g_filename_to_uri (source, NULL, NULL);

	target_uri = panel_make_unique_desktop_uri (target_dir, source_uri);
	g_free (target_dir);
	g_free (source_uri);

	g_return_if_fail (target_uri != NULL);

	target = g_filename_from_uri (target_uri, NULL, NULL);
	g_free (target_uri);

	error = NULL;
	panel_key_file_copy_and_mark_trusted (source, target, &error);

	g_free (target);

	if (error != NULL) {
		g_warning ("Problem while copying launcher to desktop: %s",
			   error->message);
		g_error_free (error);
	}
}


static void add_drawers_from_dir (CafeMenuTreeDirectory *directory,
				  int                 pos,
				  const char         *toplevel_id);

static void
add_drawers_from_alias (CafeMenuTreeAlias *alias,
			const char     *toplevel_id)
{
	gpointer item;

	switch (cafemenu_tree_alias_get_aliased_item_type (alias)) {
	case CAFEMENU_TREE_ITEM_DIRECTORY:
		item = cafemenu_tree_alias_get_directory (alias);
		add_drawers_from_dir (item,
				      G_MAXINT/2,
				      toplevel_id);
		cafemenu_tree_item_unref (item);
		break;

	case CAFEMENU_TREE_ITEM_ENTRY:
		item = cafemenu_tree_alias_get_aliased_entry (alias);
		panel_launcher_create_with_id (toplevel_id,
					       G_MAXINT/2,
					       cafemenu_tree_entry_get_desktop_file_path (item));
		cafemenu_tree_item_unref (item);
		break;

	default:
		break;
	}
}

static void
add_drawers_from_dir (CafeMenuTreeDirectory *directory,
		      int                 pos,
		      const char         *toplevel_id)
{
	const char *name;
	GIcon *icon;
	char       *attached_toplevel_id;
	CafeMenuTreeIter *iter;
	CafeMenuTreeItemType type;

	name = cafemenu_tree_directory_get_name (directory);
	icon = cafemenu_tree_directory_get_icon (directory);

	attached_toplevel_id = panel_drawer_create_with_id (toplevel_id,
							    pos,
							    icon,
							    icon != NULL,
							    name);
	if (!attached_toplevel_id)
		return;

	iter = cafemenu_tree_directory_iter (directory);
	while ((type = cafemenu_tree_iter_next (iter)) != CAFEMENU_TREE_ITEM_INVALID) {
		gpointer item;
		switch (type) {
			case CAFEMENU_TREE_ITEM_ENTRY:
				item = cafemenu_tree_iter_get_entry (iter);
				panel_launcher_create_with_id (attached_toplevel_id,
						G_MAXINT/2,
						cafemenu_tree_entry_get_desktop_file_path (item));
				cafemenu_tree_item_unref (item);
				break;

			case CAFEMENU_TREE_ITEM_DIRECTORY:
				item = cafemenu_tree_iter_get_directory (iter);
				add_drawers_from_dir (item,
						G_MAXINT/2,
						attached_toplevel_id);
				cafemenu_tree_item_unref (item);
				break;

			case CAFEMENU_TREE_ITEM_ALIAS:
				item = cafemenu_tree_iter_get_alias (iter);
				add_drawers_from_alias (item, attached_toplevel_id);
				cafemenu_tree_item_unref (item);
				break;
			default:
				break;
		}
	}
	cafemenu_tree_iter_unref (iter);
	g_free (attached_toplevel_id);
}

static void
add_menudrawer_to_panel (CtkWidget      *menuitem,
			 CafeMenuTreeEntry *entry)

{
	CafeMenuTreeDirectory *directory;
	PanelWidget       *panel;
	PanelData         *pd;
	int                insertion_pos;

	directory = cafemenu_tree_entry_get_parent (entry);

	panel = menu_get_panel (menuitem);

	pd = g_object_get_data (G_OBJECT (panel->toplevel), "PanelData");
	insertion_pos = pd ? pd->insertion_pos : -1;

	add_drawers_from_dir (directory,
			      insertion_pos,
			      panel_profile_get_toplevel_id (panel->toplevel));

	cafemenu_tree_item_unref (directory);
}

static void
add_menu_to_panel (CtkWidget      *menuitem,
		   CafeMenuTreeEntry *entry)
{
	CafeMenuTreeDirectory *directory;
	CafeMenuTree          *tree;
	PanelWidget        *panel;
	PanelData          *pd;
	int                 insertion_pos;
	char               *menu_path;
	const char         *menu_filename;

	directory = cafemenu_tree_entry_get_parent (entry);
	if (!directory) {
		g_warning ("Cannot find the filename for the menu: no directory");
		return;
	}

	tree = cafemenu_tree_directory_get_tree (directory);
	if (!tree) {
		cafemenu_tree_item_unref (directory);
		g_warning ("Cannot find the filename for the menu: no tree");
		return;
	}

	menu_filename = cafemenu_tree_get_canonical_menu_path (tree);
	g_object_unref (tree);
	if (!menu_filename) {
		cafemenu_tree_item_unref (directory);
		g_warning ("Cannot find the filename for the menu: no filename");
		return;
	}

	panel = menu_get_panel (menuitem);

	pd = g_object_get_data (G_OBJECT (panel->toplevel), "PanelData");
	insertion_pos = pd ? pd->insertion_pos : -1;

	menu_path = cafemenu_tree_directory_make_path (directory, NULL);

	panel_menu_button_create (panel->toplevel,
				  insertion_pos,
				  menu_filename,
				  menu_path,
				  TRUE,
				  cafemenu_tree_directory_get_name (directory));

	g_free (menu_path);

	cafemenu_tree_item_unref (directory);
}

static void
grab_widget (CtkWidget *widget)
{
	g_return_if_fail (widget != NULL);

	CdkWindow *window;
	CdkDisplay *display;
	CdkSeat *seat;

	window = ctk_widget_get_window (widget);
	display = cdk_window_get_display (window);

	seat = cdk_display_get_default_seat (display);
	cdk_seat_grab (seat, window,
	               CDK_SEAT_CAPABILITY_ALL, TRUE,
	               NULL, NULL, NULL, NULL);
}

static void
restore_grabs (CtkWidget *menu G_GNUC_UNUSED,
	       CtkWidget *item)
{
	CtkWidget *parent = ctk_widget_get_parent (item);

	if (parent) {
		grab_widget (parent);
	}
}

static void
menu_destroy_context_menu (CtkWidget *item,
                           CtkWidget *menu)
{
	g_signal_handlers_disconnect_by_func (menu, restore_grabs, item);
	ctk_widget_destroy (menu);
}

static CtkWidget *
create_item_context_menu (CtkWidget   *item,
			  PanelWidget *panel_widget)
{
	CafeMenuTreeEntry     *entry;
	CafeMenuTreeDirectory *directory;
	CafeMenuTree          *tree;
	CtkWidget          *menu;
	CtkWidget          *submenu;
	CtkWidget          *menuitem;
	const char         *menu_filename;
	gboolean            id_lists_writable;

	id_lists_writable = panel_profile_id_lists_are_writable ();

	entry = g_object_get_data (G_OBJECT (item), "panel-menu-tree-entry");
	if (!entry)
		return NULL;

	directory = cafemenu_tree_entry_get_parent (entry);
	if (!directory)
		return NULL;

	tree = cafemenu_tree_directory_get_tree (directory);
	cafemenu_tree_item_unref (directory);
	if (!tree)
		return NULL;

	menu_filename = cafemenu_tree_get_canonical_menu_path(tree);
	g_object_unref (tree);
	if (!menu_filename)
		return NULL;

	menu = create_empty_menu ();
	g_object_set_data (G_OBJECT (item), "panel-item-context-menu", menu);
	g_object_set_data (G_OBJECT (menu), "menu_panel", panel_widget);

	g_signal_connect (item, "destroy",
			  G_CALLBACK (menu_destroy_context_menu), menu);
	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (restore_grabs), item);

	menuitem = ctk_menu_item_new_with_mnemonic (_("Add this launcher to _panel"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_app_to_panel), entry);
	ctk_widget_set_sensitive (menuitem, id_lists_writable);
	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
	ctk_widget_show (menuitem);

	menuitem = ctk_menu_item_new_with_mnemonic (_("Add this launcher to _desktop"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_app_to_desktop), entry);
	ctk_widget_set_sensitive (menuitem, id_lists_writable);
	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
	ctk_widget_show (menuitem);


	submenu = create_empty_menu ();

	g_object_set_data (G_OBJECT (submenu), "menu_panel", panel_widget);

	menuitem = ctk_menu_item_new_with_mnemonic (_("_Entire menu"));
	ctk_menu_item_set_submenu (CTK_MENU_ITEM (menuitem), submenu);
	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
	ctk_widget_show (menuitem);

	menuitem = ctk_menu_item_new_with_mnemonic (_("Add this as _drawer to panel"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_menudrawer_to_panel), entry);
	ctk_widget_set_sensitive (menuitem, id_lists_writable);
	ctk_menu_shell_append (CTK_MENU_SHELL (submenu), menuitem);
	ctk_widget_show (menuitem);

	menuitem = ctk_menu_item_new_with_mnemonic (_("Add this as _menu to panel"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_menu_to_panel), entry);
	ctk_widget_set_sensitive (menuitem, id_lists_writable);
	ctk_menu_shell_append (CTK_MENU_SHELL (submenu), menuitem);
	ctk_widget_show (menuitem);

	return menu;
}

static gboolean
show_item_menu (CtkWidget      *item,
		CdkEvent *event)
{
	PanelWidget *panel_widget;
	CtkWidget   *menu;

	if (panel_lockdown_get_locked_down ())
		return FALSE;

	panel_widget = menu_get_panel (item);

	menu = g_object_get_data (G_OBJECT (item), "panel-item-context-menu");

	if (!menu)
		menu = create_item_context_menu (item, panel_widget);

	if (!menu)
		return FALSE;

	ctk_menu_set_screen (CTK_MENU (menu),
			     ctk_window_get_screen (CTK_WINDOW (panel_widget->toplevel)));
	/* Set up theme and transparency support */
	CtkWidget *toplevel = ctk_widget_get_toplevel (menu);
	/* Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	CdkScreen *screen = ctk_widget_get_screen (CTK_WIDGET (toplevel));
	CdkVisual *visual = cdk_screen_get_rgba_visual (screen);
	ctk_widget_set_visual(CTK_WIDGET (toplevel), visual);
	/* Set menu and it's toplevel window to follow panel theme */
	CtkStyleContext *context;
	context = ctk_widget_get_style_context (CTK_WIDGET (toplevel));
	ctk_style_context_add_class(context,"gnome-panel-menu-bar");
	ctk_style_context_add_class(context,"cafe-panel-menu-bar");
	ctk_menu_popup_at_pointer (CTK_MENU (menu), event);

	return TRUE;
}

gboolean
menu_dummy_button_press_event (CtkWidget      *menuitem G_GNUC_UNUSED,
			       CdkEventButton *event)
{
	if (event->button == 3)
		return TRUE;

	return FALSE;
}

static gboolean
menuitem_button_press_event (CtkWidget      *menuitem,
			     CdkEventButton *event)
{
	if (event->button == 3)
		return show_item_menu (menuitem, (CdkEvent *) event);

	return FALSE;
}

static void
drag_begin_menu_cb (CtkWidget      *widget,
		    CdkDragContext *context G_GNUC_UNUSED)
{
	/* FIXME: workaround for a possible ctk+ bug
	 *    See bugs #92085(ctk+) and #91184(panel) for details.
	 *    Maybe it's not needed with CtkTooltip?
	 */
	g_object_set (widget, "has-tooltip", FALSE, NULL);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * CTK+ menuing code in some manner.
 */
static void
drag_end_menu_cb (CtkWidget      *widget,
		  CdkDragContext *context G_GNUC_UNUSED)
{
  CtkWidget *xgrab_shell;
  CtkWidget *parent;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = ctk_widget_get_parent (widget);
  xgrab_shell = NULL;

  /* FIXME: workaround for a possible ctk+ bug
   *    See bugs #92085(ctk+) and #91184(panel) for details.
   */
  g_object_set (widget, "has-tooltip", TRUE, NULL);

  while (parent)
    {
      gboolean viewable = TRUE;
      CtkWidget *tmp = parent;

      while (tmp)
	{
	  if (!ctk_widget_get_mapped (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = ctk_widget_get_parent (tmp);
	}

      if (viewable)
	xgrab_shell = parent;

      parent = ctk_menu_shell_get_parent_shell (CTK_MENU_SHELL (parent));
    }

  if (xgrab_shell)
    {
      grab_widget (xgrab_shell);
    }
}

static void
drag_data_get_menu_cb (CtkWidget         *widget G_GNUC_UNUSED,
		       CdkDragContext    *context G_GNUC_UNUSED,
		       CtkSelectionData  *selection_data,
		       guint              info G_GNUC_UNUSED,
		       guint              time G_GNUC_UNUSED,
		       CafeMenuTreeEntry *entry)
{
	const char *path;
	char       *uri;
	char       *uri_list;

	path = cafemenu_tree_entry_get_desktop_file_path (entry);
	uri = g_filename_to_uri (path, NULL, NULL);
	uri_list = g_strconcat (uri, "\r\n", NULL);
	g_free (uri);

	ctk_selection_data_set (selection_data,
				ctk_selection_data_get_target (selection_data), 8, (guchar *)uri_list,
				strlen (uri_list));
	g_free (uri_list);
}

static char *
menu_escape_underscores_and_prepend (const char *text)
{
	GString    *escaped_text;
	const char *src;
	int         inserted;

	if (!text)
		return g_strdup (text);

	escaped_text = g_string_sized_new (strlen (text) + 1);
	g_string_printf (escaped_text, "_%s", text);

	src = text;
	inserted = 1;

	while (*src) {
		gunichar c;

		c = g_utf8_get_char (src);

		if (c == (gunichar)-1) {
			g_warning ("Invalid input string for underscore escaping");
			g_string_free (escaped_text, TRUE);
			return g_strdup (text);
		} else if (c == '_') {
			g_string_insert_c (escaped_text,
					   src - text + inserted, '_');
			inserted++;
		}

		src = g_utf8_next_char (src);
	}

	return g_string_free (escaped_text, FALSE);
}

void
setup_menuitem_with_icon (CtkWidget   *menuitem,
			  CtkIconSize  icon_size,
			  GIcon       *gicon,
			  const char  *image_filename,
			  const char  *title)
{
	CtkWidget *image;
	GIcon *icon = NULL;

	image = ctk_image_new ();
	g_object_set (image, "icon-size", icon_size, NULL);

	if (gicon)
		icon = g_object_ref (gicon);
	else if (image_filename)
		icon = panel_gicon_from_icon_name (image_filename);

	ctk_image_set_from_gicon (CTK_IMAGE(image), icon, icon_size);
	g_clear_object (&icon);

	ctk_widget_show (image);

	setup_menuitem (menuitem, icon_size, image, title);
}

void
setup_menuitem (CtkWidget   *menuitem,
		CtkIconSize  icon_size,
		CtkWidget   *image,
		const char  *title)

{
	CtkWidget *label;
	char      *_title;

	/* this creates a label with an invisible mnemonic */
	label = g_object_new (CTK_TYPE_ACCEL_LABEL, NULL);
	_title = menu_escape_underscores_and_prepend (title);
	ctk_label_set_text_with_mnemonic (CTK_LABEL (label), _title);
	g_free (_title);

	ctk_label_set_pattern (CTK_LABEL (label), "");

	ctk_accel_label_set_accel_widget (CTK_ACCEL_LABEL (label), menuitem);

	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_widget_show (label);

	ctk_container_add (CTK_CONTAINER (menuitem), label);

	if (image) {
		gint icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

		ctk_icon_size_lookup (icon_size, NULL, &icon_height);
		ctk_widget_show (image);
		ctk_image_menu_item_set_image (CTK_IMAGE_MENU_ITEM (menuitem),
					       image);
		ctk_image_set_pixel_size (CTK_IMAGE(image), icon_height);
	}

	ctk_widget_show (menuitem);
}

static void
drag_data_get_string_cb (CtkWidget        *widget G_GNUC_UNUSED,
			 CdkDragContext   *context G_GNUC_UNUSED,
			 CtkSelectionData *selection_data,
			 guint             info G_GNUC_UNUSED,
			 guint             time G_GNUC_UNUSED,
			 const char       *string)
{
	ctk_selection_data_set (selection_data,
				ctk_selection_data_get_target (selection_data), 8, (guchar *)string,
				strlen(string));
}

void
setup_uri_drag (CtkWidget  *menuitem,
		const char *uri,
		const char *icon,
		CdkDragAction action)
{
	static CtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	if (panel_lockdown_get_locked_down ())
		return;

	ctk_drag_source_set (menuitem,
			     CDK_BUTTON1_MASK|CDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     action);

	if (icon != NULL)
		ctk_drag_source_set_icon_name (menuitem, icon);

	g_signal_connect (G_OBJECT (menuitem), "drag_begin",
			  G_CALLBACK (drag_begin_menu_cb), NULL);
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (uri),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);
}

void
setup_internal_applet_drag (CtkWidget             *menuitem,
			    PanelActionButtonType  type)
{
	static CtkTargetEntry menu_item_targets[] = {
		{ "application/x-cafe-panel-applet-internal", 0, 0 }
	};

	if (panel_lockdown_get_locked_down ())
		return;

	ctk_drag_source_set (menuitem,
			     CDK_BUTTON1_MASK|CDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     CDK_ACTION_COPY);

	if (panel_action_get_icon_name (type)  != NULL)
		ctk_drag_source_set_icon_name (menuitem,
					       panel_action_get_icon_name (type));

	g_signal_connect (G_OBJECT (menuitem), "drag_begin",
			  G_CALLBACK (drag_begin_menu_cb), NULL);
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (panel_action_get_drag_id (type)),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);
}

static void
submenu_to_display (CtkWidget *menu)
{
	CafeMenuTreeDirectory  *directory;
	void               (*append_callback) (CtkWidget *, gpointer);
	gpointer             append_data;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-loading"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-loading", NULL);

	directory = g_object_get_data (G_OBJECT (menu),
				       "panel-menu-tree-directory");
	if (!directory) {
		CafeMenuTree *tree;
		const char   *menu_path;

		menu_path = g_object_get_data (G_OBJECT (menu),
					       "panel-menu-tree-path");
		if (!menu_path)
			return;

		tree = g_object_get_data (G_OBJECT (menu), "panel-menu-tree");
		if (!tree)
			return;

		directory = cafemenu_tree_get_directory_from_path (tree,
								menu_path);

		g_object_set_data_full (G_OBJECT (menu),
					"panel-menu-tree-directory",
					directory,
					(GDestroyNotify) cafemenu_tree_item_unref);
	}

	if (directory)
		populate_menu_from_directory (menu, directory);

	append_callback = g_object_get_data (G_OBJECT (menu),
					     "panel-menu-append-callback");
	append_data     = g_object_get_data (G_OBJECT (menu),
					     "panel-menu-append-callback-data");
	if (append_callback)
		append_callback (menu, append_data);
}

static gboolean
submenu_to_display_in_idle (gpointer data)
{
	CtkWidget *menu = CTK_WIDGET (data);

	g_object_set_data (G_OBJECT (menu), "panel-menu-idle-id", NULL);

	submenu_to_display (menu);

	return FALSE;
}

static void
remove_submenu_to_display_idle (gpointer data)
{
	guint idle_id = GPOINTER_TO_UINT (data);

	g_source_remove (idle_id);
}

static CtkWidget *
create_fake_menu (CafeMenuTreeDirectory *directory)
{
	CtkWidget *menu;
	guint      idle_id;

	menu = create_empty_menu ();

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				cafemenu_tree_item_ref (directory),
				(GDestroyNotify) cafemenu_tree_item_unref);

	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);

	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);


/* Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	CtkWidget *toplevel = ctk_widget_get_toplevel (menu);
	CdkScreen *screen = ctk_widget_get_screen(CTK_WIDGET(toplevel));
	CdkVisual *visual = cdk_screen_get_rgba_visual(screen);
	ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);

	return menu;
}
CtkWidget *
panel_image_menu_item_new (void)
{
	CtkWidget *menuitem;

	menuitem = ctk_image_menu_item_new ();
	ctk_image_menu_item_set_always_show_image (CTK_IMAGE_MENU_ITEM (menuitem),
						   TRUE);
	return menuitem;
}

static CtkWidget *
create_submenu_entry (CtkWidget          *menu,
		      CafeMenuTreeDirectory *directory)
{
	CtkWidget *menuitem;
	gboolean   force_categories_icon;

	force_categories_icon = g_object_get_data (G_OBJECT (menu),
						   "panel-menu-force-icon-for-categories") != NULL;

	if (force_categories_icon)
		menuitem = panel_image_menu_item_new ();
	else
		menuitem = ctk_image_menu_item_new ();

	setup_menuitem_with_icon (menuitem,
				  panel_menu_icon_get_size (),
				  cafemenu_tree_directory_get_icon (directory),
				  NULL,
				  cafemenu_tree_directory_get_name (directory));

	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);

	ctk_widget_show (menuitem);

	return menuitem;
}

static void
create_submenu (CtkWidget          *menu,
		CafeMenuTreeDirectory *directory,
		CafeMenuTreeDirectory *alias_directory)
{
	CtkWidget *menuitem;
	CtkWidget *submenu;
	gboolean   force_categories_icon;

	if (alias_directory)
		menuitem = create_submenu_entry (menu, alias_directory);
	else
		menuitem = create_submenu_entry (menu, directory);

	submenu = create_fake_menu (directory);

	ctk_menu_item_set_submenu (CTK_MENU_ITEM (menuitem), submenu);

	/* Keep the infor that we force (or not) the icons to be visible */
	force_categories_icon = g_object_get_data (G_OBJECT (menu),
						   "panel-menu-force-icon-for-categories") != NULL;
	g_object_set_data (G_OBJECT (submenu),
			   "panel-menu-force-icon-for-categories",
			   GINT_TO_POINTER (force_categories_icon));
}

static void
create_header (CtkWidget       *menu,
	       CafeMenuTreeHeader *header)
{
	CafeMenuTreeDirectory *directory;
	CtkWidget          *menuitem;

	directory = cafemenu_tree_header_get_directory (header);
	menuitem = create_submenu_entry (menu, directory);
	cafemenu_tree_item_unref (directory);

	g_object_set_data_full (G_OBJECT (menuitem),
				"panel-cafemenu-tree.header",
				cafemenu_tree_item_ref (header),
				(GDestroyNotify) cafemenu_tree_item_unref);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (ctk_false), NULL);
}

static void
create_menuitem (CtkWidget          *menu,
		 CafeMenuTreeEntry     *entry,
		 CafeMenuTreeDirectory *alias_directory)
{
	CtkWidget  *menuitem;

	menuitem = panel_image_menu_item_new ();

	g_object_set_data_full (G_OBJECT (menuitem),
				"panel-menu-tree-entry",
				cafemenu_tree_item_ref (entry),
				(GDestroyNotify) cafemenu_tree_item_unref);

	if (alias_directory) {
		//FIXME: we should probably use this data when we do dnd or
		//context menu for this menu item
		g_object_set_data_full (G_OBJECT (menuitem),
					"panel-menu-tree-alias-directory",
					cafemenu_tree_item_ref (alias_directory),
					(GDestroyNotify) cafemenu_tree_item_unref);

		setup_menuitem_with_icon (menuitem,
				panel_menu_icon_get_size (),
				cafemenu_tree_directory_get_icon (alias_directory),
				NULL,
				cafemenu_tree_directory_get_name (alias_directory));
		if (cafemenu_tree_directory_get_comment (alias_directory)) {
			panel_util_set_tooltip_text (menuitem, cafemenu_tree_directory_get_comment (alias_directory));
		}
	} else {
		GDesktopAppInfo *ginfo;
		const gchar* desc;
		const gchar* gename;

		ginfo = cafemenu_tree_entry_get_app_info (entry);
		desc= g_app_info_get_description(G_APP_INFO(ginfo));
		gename = g_desktop_app_info_get_generic_name(ginfo);

		setup_menuitem_with_icon (menuitem,
				panel_menu_icon_get_size (),
				g_app_info_get_icon(G_APP_INFO(ginfo)),
				NULL,
				g_app_info_get_name(G_APP_INFO(ginfo)));

		if (desc != NULL) {
			panel_util_set_tooltip_text (menuitem, desc);
		} else if ( gename != NULL) {
			panel_util_set_tooltip_text (menuitem, gename);
		}
	}

	g_signal_connect_after (menuitem, "button_press_event",
				G_CALLBACK (menuitem_button_press_event), NULL);

	if (!panel_lockdown_get_locked_down ()) {
		static CtkTargetEntry menu_item_targets[] = {
			{ "text/uri-list", 0, 0 }
		};

		ctk_drag_source_set (menuitem,
				     CDK_BUTTON1_MASK | CDK_BUTTON2_MASK,
				     menu_item_targets, 1,
				     CDK_ACTION_COPY);

		GDesktopAppInfo *ginfo;
		ginfo = cafemenu_tree_entry_get_app_info (entry);
		if (g_app_info_get_icon (G_APP_INFO(ginfo)) != NULL) {
			GIcon      *gicon;

			gicon = g_app_info_get_icon (G_APP_INFO(ginfo));
			if (gicon != NULL) {
				ctk_drag_source_set_icon_gicon (menuitem, gicon);
			}
		}

		g_signal_connect (G_OBJECT (menuitem), "drag_begin",
				  G_CALLBACK (drag_begin_menu_cb), NULL);
		g_signal_connect (menuitem, "drag_data_get",
				  G_CALLBACK (drag_data_get_menu_cb), entry);
		g_signal_connect (menuitem, "drag_end",
				  G_CALLBACK (drag_end_menu_cb), NULL);
	}

	ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (activate_app_def), entry);

	ctk_widget_show (menuitem);
}

static void
create_menuitem_from_alias (CtkWidget      *menu,
			    CafeMenuTreeAlias *alias)
{
	gpointer item, entry;

	switch (cafemenu_tree_alias_get_aliased_item_type (alias)) {
	case CAFEMENU_TREE_ITEM_DIRECTORY:
		item = cafemenu_tree_alias_get_directory (alias);
		create_submenu (menu, item, item);
		cafemenu_tree_item_unref (item);
		break;

	case CAFEMENU_TREE_ITEM_ENTRY:
		entry = cafemenu_tree_alias_get_aliased_entry(alias);
		item = cafemenu_tree_alias_get_directory (alias);
		create_menuitem (menu, entry, item);
		cafemenu_tree_item_unref (entry);
		cafemenu_tree_item_unref (item);
		break;

	default:
		break;
	}
}

static void
handle_cafemenu_tree_changed (CafeMenuTree *tree,
			   CtkWidget *menu)
{
	GError *error = NULL;
	guint idle_id;

	GList *list, *l;
	list = ctk_container_get_children (CTK_CONTAINER (menu));
	for (l = list; l; l = l->next)
		ctk_widget_destroy (l->data);
	g_list_free (list);

	if (! cafemenu_tree_load_sync (tree, &error)) {
		g_warning("Menu tree reload got error:%s\n", error->message);
		g_error_free(error);
	}

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				NULL, NULL);

	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);
}

static void
remove_cafemenu_tree_monitor (CtkWidget *menu,
                          CafeMenuTree  *tree)
{
        g_signal_handlers_disconnect_by_func (tree,
                                              G_CALLBACK (handle_cafemenu_tree_changed),
                                              menu);
}

CtkWidget *
create_applications_menu (const char *menu_file,
			  const char *menu_path,
			  gboolean    always_show_image)
{
	CafeMenuTree *tree;
	CtkWidget *menu;
	guint      idle_id;
	GError *error = NULL;

	menu = create_empty_menu ();

	if (always_show_image)
		g_object_set_data (G_OBJECT (menu),
				   "panel-menu-force-icon-for-categories",
				   GINT_TO_POINTER (TRUE));

	tree = cafemenu_tree_new (menu_file, CAFEMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
	if (! cafemenu_tree_load_sync (tree, &error)) {
		g_warning("Menu tree loading got error:%s\n", error->message);
		g_error_free(error);
		g_object_unref(tree);
		tree = NULL;
	}

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree",
				g_object_ref(tree),
				(GDestroyNotify) g_object_unref);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-path",
				g_strdup (menu_path ? menu_path : "/"),
				(GDestroyNotify) g_free);

	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);

	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	g_signal_connect (tree, "changed", G_CALLBACK (handle_cafemenu_tree_changed), menu);
	g_signal_connect (menu, "destroy", G_CALLBACK (remove_cafemenu_tree_monitor), tree);

	g_object_unref(tree);

/*HACK Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	CtkWidget *toplevel = ctk_widget_get_toplevel (menu);
	CdkScreen *screen = ctk_widget_get_screen(CTK_WIDGET(toplevel));
	CdkVisual *visual = cdk_screen_get_rgba_visual(screen);
	ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);

	return menu;
}

static CtkWidget *
populate_menu_from_directory (CtkWidget          *menu,
			      CafeMenuTreeDirectory *directory)
{
	GList    *children;
	gboolean  add_separator;
	CafeMenuTreeIter *iter;
	CafeMenuTreeItemType type;

	children = ctk_container_get_children (CTK_CONTAINER (menu));
	add_separator = (children != NULL);
	g_list_free (children);

	iter = cafemenu_tree_directory_iter (directory);
	while ((type = cafemenu_tree_iter_next (iter)) != CAFEMENU_TREE_ITEM_INVALID) {
		gpointer item;
		if (add_separator || type == CAFEMENU_TREE_ITEM_SEPARATOR) {
			add_menu_separator (menu);
			add_separator = FALSE;
		}
		switch (type) {
			case CAFEMENU_TREE_ITEM_DIRECTORY:
				item = cafemenu_tree_iter_get_directory(iter);
				create_submenu (menu, item, NULL);
				cafemenu_tree_item_unref (item);
				break;

			case CAFEMENU_TREE_ITEM_ENTRY:
				item = cafemenu_tree_iter_get_entry (iter);
				create_menuitem (menu, item, NULL);
				cafemenu_tree_item_unref (item);
				break;

			case CAFEMENU_TREE_ITEM_SEPARATOR :
				/* already added */
				break;

			case CAFEMENU_TREE_ITEM_ALIAS:
				item = cafemenu_tree_iter_get_alias(iter);
				create_menuitem_from_alias (menu, item);
				cafemenu_tree_item_unref (item);
				break;

			case CAFEMENU_TREE_ITEM_HEADER:
				item = cafemenu_tree_iter_get_header(iter);
				create_header (menu, item);
				cafemenu_tree_item_unref (item);
				break;

			default:
				break;
		}
	}
	cafemenu_tree_iter_unref (iter);

	return menu;
}

static void
main_menu_append (CtkWidget *main_menu,
		  gpointer   data)
{
	PanelWidget *panel;
	CtkWidget   *item;
	gboolean     add_separator;
	GList       *children;
	GList       *last;

	panel = PANEL_WIDGET (data);

	add_separator = FALSE;
	children = ctk_container_get_children (CTK_CONTAINER (main_menu));
	last = g_list_last (children);
	if (last != NULL) {
		add_separator = !CTK_IS_SEPARATOR (CTK_WIDGET (last->data));
	}
	g_list_free (children);

	if (add_separator)
		add_menu_separator (main_menu);

	item = panel_place_menu_item_new (TRUE);
	panel_place_menu_item_set_panel (item, panel);
	ctk_menu_shell_append (CTK_MENU_SHELL (main_menu), item);
	ctk_widget_show (item);

	item = panel_desktop_menu_item_new (TRUE, FALSE);
	panel_desktop_menu_item_set_panel (item, panel);
	ctk_menu_shell_append (CTK_MENU_SHELL (main_menu), item);
	ctk_widget_show (item);

	panel_menu_items_append_lock_logout (main_menu);
}

CtkWidget* create_main_menu(PanelWidget* panel)
{
	CtkWidget* main_menu;

	main_menu = create_applications_menu("cafe-applications.menu", NULL, TRUE);

	g_object_set_data(G_OBJECT(main_menu), "menu_panel", panel);
	/* FIXME need to update the panel on parent_set */

	g_object_set_data(G_OBJECT(main_menu), "panel-menu-append-callback", main_menu_append);
	g_object_set_data(G_OBJECT(main_menu), "panel-menu-append-callback-data", panel);

	return main_menu;
}

static gboolean
panel_menu_key_press_handler (CtkWidget   *widget,
			      CdkEventKey *event)
{
	gboolean retval = FALSE;
	CtkWidget *active_menu_item = NULL;

	if ((event->keyval == CDK_KEY_Menu) ||
	    (event->keyval == CDK_KEY_F10 &&
	    (event->state & ctk_accelerator_get_default_mod_mask ()) == CDK_SHIFT_MASK)) {
		CtkMenuShell *menu_shell = CTK_MENU_SHELL (widget);

		active_menu_item = ctk_menu_shell_get_selected_item (menu_shell);
		if (active_menu_item && ctk_menu_item_get_submenu (CTK_MENU_ITEM (active_menu_item)) == NULL)
			retval = show_item_menu (active_menu_item, (CdkEvent *) event);
	}
	return retval;
}
