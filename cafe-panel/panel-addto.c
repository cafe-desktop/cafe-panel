/*
 * panel-addto.c:
 *
 * Copyright (C) 2004 Vincent Untz
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
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <cafemenu-tree.h>

#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-show.h>
#include <libpanel-util/panel-ctk.h>

#include "launcher.h"
#include "panel.h"
#include "drawer.h"
#include "panel-applets-manager.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-separator.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-lockdown.h"
#include "panel-util.h"
#include "panel-profile.h"
#include "panel-addto.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"
#include "panel-stock-icons.h"

#ifdef HAVE_X11
#include "xstuff.h"
#endif
#ifdef HAVE_WAYLAND
#include "cdk/cdkwayland.h"
#endif

typedef struct {
	PanelWidget *panel_widget;

	CtkWidget    *addto_dialog;
	CtkWidget    *label;
	CtkWidget    *search_entry;
	CtkWidget    *back_button;
	CtkWidget    *add_button;
	CtkWidget    *tree_view;
	CtkTreeModel *applet_model;
	CtkTreeModel *filter_applet_model;
	CtkTreeModel *application_model;
	CtkTreeModel *filter_application_model;

	CafeMenuTree    *menu_tree;

	GSList       *applet_list;
	GSList       *application_list;
	GSList       *settings_list;

	gchar        *search_text;
	gchar        *applet_search_text;

	int           insertion_position;
} PanelAddtoDialog;

static GQuark panel_addto_dialog_quark = 0;

typedef enum {
	PANEL_ADDTO_APPLET,
	PANEL_ADDTO_ACTION,
	PANEL_ADDTO_LAUNCHER_MENU,
	PANEL_ADDTO_LAUNCHER,
	PANEL_ADDTO_LAUNCHER_NEW,
	PANEL_ADDTO_MENU,
	PANEL_ADDTO_MENUBAR,
	PANEL_ADDTO_SEPARATOR,
	PANEL_ADDTO_DRAWER
} PanelAddtoItemType;

typedef struct {
	PanelAddtoItemType     type;
	char                  *name;
	char                  *description;
	char                  *icon;
	PanelActionButtonType  action_type;
	char                  *launcher_path;
	char                  *menu_filename;
	char                  *menu_path;
	char                  *iid;
	gboolean               enabled;
	gboolean               static_data;
} PanelAddtoItemInfo;

typedef struct {
	GSList             *children;
	PanelAddtoItemInfo  item_info;
} PanelAddtoAppList;

static PanelAddtoItemInfo special_addto_items [] = {

	{ PANEL_ADDTO_LAUNCHER_NEW,
	  N_("Custom Application Launcher"),
	  N_("Create a new launcher"),
	  PANEL_ICON_LAUNCHER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "LAUNCHER:ASK",
	  TRUE,
	  TRUE },

	{ PANEL_ADDTO_LAUNCHER_MENU,
	  N_("Application Launcher..."),
	  N_("Copy a launcher from the applications menu"),
	  PANEL_ICON_LAUNCHER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "LAUNCHER:MENU",
	  TRUE,
	  TRUE }

};

static PanelAddtoItemInfo internal_addto_items [] = {

	{ PANEL_ADDTO_MENU,
	  N_("Compact Menu"),
	  N_("A compact menu"),
	  PANEL_ICON_MAIN_MENU,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "MENU:MAIN",
	  TRUE,
	  TRUE },

	{ PANEL_ADDTO_MENUBAR,
	  N_("Classic Menu"),
	  N_("The classic Applications, Places and System menu bar"),
	  PANEL_ICON_MAIN_MENU,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "MENUBAR:NEW",
	  TRUE,
	  TRUE },

	{ PANEL_ADDTO_SEPARATOR,
	  N_("Separator"),
	  N_("A separator to organize the panel items"),
	  PANEL_ICON_SEPARATOR,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "SEPARATOR:NEW",
	  TRUE,
	  TRUE },

	{ PANEL_ADDTO_DRAWER,
	  N_("Drawer"),
	  N_("A pop out drawer to store other items in"),
	  PANEL_ICON_DRAWER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  NULL,
	  "DRAWER:NEW",
	  TRUE,
	  TRUE }
};

enum {
	COLUMN_ICON_NAME,
	COLUMN_TEXT,
	COLUMN_DATA,
	COLUMN_SEARCH,
	COLUMN_ENABLED,
	NUMBER_COLUMNS
};

enum {
	PANEL_ADDTO_RESPONSE_BACK,
	PANEL_ADDTO_RESPONSE_ADD
};

static void panel_addto_present_applications (PanelAddtoDialog *dialog);
static void panel_addto_present_applets      (PanelAddtoDialog *dialog);
static gboolean panel_addto_filter_func (CtkTreeModel *model,
					 CtkTreeIter  *iter,
					 gpointer      data);

static int
panel_addto_applet_info_sort_func (PanelAddtoItemInfo *a,
				   PanelAddtoItemInfo *b)
{
	return g_utf8_collate (a->name, b->name);
}

static GSList *
panel_addto_prepend_internal_applets (GSList *list)
{
	static gboolean translated = FALSE;
	int             i;

	for (i = 0; i < G_N_ELEMENTS (internal_addto_items); i++) {
		if (!translated) {
			internal_addto_items [i].name        = _(internal_addto_items [i].name);
			internal_addto_items [i].description = _(internal_addto_items [i].description);
		}

                list = g_slist_prepend (list, &internal_addto_items [i]);
        }

	translated = TRUE;

	for (i = PANEL_ACTION_LOCK; i < PANEL_ACTION_LAST; i++) {
		PanelAddtoItemInfo *info;

		if (panel_action_get_is_disabled (i))
			continue;

		info              = g_new0 (PanelAddtoItemInfo, 1);
		info->type        = PANEL_ADDTO_ACTION;
		info->action_type = i;
		info->name        = g_strdup (panel_action_get_text (i));
		info->description = g_strdup (panel_action_get_tooltip (i));
		info->icon        = g_strdup (panel_action_get_icon_name (i));
		info->iid         = g_strdup (panel_action_get_drag_id (i));
		info->enabled     = TRUE;
		info->static_data = FALSE;

		list = g_slist_prepend (list, info);
	}

        return list;
}

static char *
panel_addto_make_text (const char *name,
		       const char *desc)
{
	const char *real_name;
	char       *result;

	real_name = name ? name : _("(empty)");

	if (!PANEL_GLIB_STR_EMPTY (desc)) {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n%s",
						  real_name, desc);
	} else {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
						  real_name);
	}

	return result;
}

static void
panel_addto_drag_data_get_cb (CtkWidget        *widget,
			      GdkDragContext   *context,
			      CtkSelectionData *selection_data,
			      guint             info,
			      guint             time,
			      const char       *string)
{
	ctk_selection_data_set (selection_data,
				ctk_selection_data_get_target (selection_data), 8,
				(guchar *) string, strlen (string));
}

static void
panel_addto_drag_begin_cb (CtkWidget      *widget,
			   GdkDragContext *context,
			   gpointer        data)
{
	CtkTreeModel *filter_model;
	CtkTreeModel *child_model;
	CtkTreePath  *path;
	CtkTreeIter   iter;
	CtkTreeIter   filter_iter;
	gchar         *icon_name;

	filter_model = ctk_tree_view_get_model (CTK_TREE_VIEW (widget));

	ctk_tree_view_get_cursor (CTK_TREE_VIEW (widget), &path, NULL);
	ctk_tree_model_get_iter (filter_model, &filter_iter, path);
	ctk_tree_path_free (path);
	ctk_tree_model_filter_convert_iter_to_child_iter (CTK_TREE_MODEL_FILTER (filter_model),
							  &iter, &filter_iter);

	child_model = ctk_tree_model_filter_get_model (CTK_TREE_MODEL_FILTER (filter_model));
	ctk_tree_model_get (child_model, &iter,
	                    COLUMN_ICON_NAME, &icon_name,
	                    -1);

	ctk_drag_set_icon_name (context, icon_name, 0, 0);
	g_free (icon_name);
}

static void
panel_addto_setup_drag (CtkTreeView          *tree_view,
			const CtkTargetEntry *target,
			const char           *text)
{
	if (!text || panel_lockdown_get_locked_down ())
		return;

	ctk_tree_view_enable_model_drag_source (CTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
						target, 1, GDK_ACTION_COPY);

	g_signal_connect_data (G_OBJECT (tree_view), "drag_data_get",
			       G_CALLBACK (panel_addto_drag_data_get_cb),
			       g_strdup (text),
			       (GClosureNotify) g_free,
			       0 /* connect_flags */);
	g_signal_connect_after (G_OBJECT (tree_view), "drag-begin",
	                        G_CALLBACK (panel_addto_drag_begin_cb),
	                        NULL);
}

static void
panel_addto_setup_launcher_drag (CtkTreeView *tree_view,
				 const char  *path)
{
        static CtkTargetEntry target[] = {
		{ "text/uri-list", 0, 0 }
	};
	char *uri;
	char *uri_list;

	uri = g_filename_to_uri (path, NULL, NULL);
	uri_list = g_strconcat (uri, "\r\n", NULL);
	panel_addto_setup_drag (tree_view, target, uri_list);
	g_free (uri_list);
	g_free (uri);
}

static void
panel_addto_setup_applet_drag (CtkTreeView *tree_view,
			       const char  *iid)
{
	static CtkTargetEntry target[] = {
		{ "application/x-cafe-panel-applet-iid", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, iid);
}

static void
panel_addto_setup_internal_applet_drag (CtkTreeView *tree_view,
					const char  *applet_type)
{
	static CtkTargetEntry target[] = {
		{ "application/x-cafe-panel-applet-internal", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, applet_type);
}

static GSList *
panel_addto_query_applets (GSList *list)
{
	GList *applet_list, *l;

	applet_list = cafe_panel_applets_manager_get_applets ();

	for (l = applet_list; l; l = g_list_next (l)) {
		CafePanelAppletInfo *info;
		const char *iid, *name, *description, *icon;
		gboolean enabled;
		PanelAddtoItemInfo *applet;

		info = (CafePanelAppletInfo *)l->data;

		iid = cafe_panel_applet_info_get_iid (info);
		name = cafe_panel_applet_info_get_name (info);
		description = cafe_panel_applet_info_get_description (info);
		icon = cafe_panel_applet_info_get_icon (info);

		if (!name || panel_lockdown_is_applet_disabled (iid)) {
			continue;
		}

		enabled = TRUE;
#ifdef HAVE_X11
		if (GDK_IS_X11_DISPLAY (cdk_display_get_default ()) &&
		    !cafe_panel_applet_info_get_x11_supported (info)) {
			enabled = FALSE;
			description = _("Not compatible with X11");
		}
#endif
#ifdef HAVE_WAYLAND
		if (GDK_IS_WAYLAND_DISPLAY (cdk_display_get_default ()) &&
		    !cafe_panel_applet_info_get_wayland_supported (info)) {
			enabled = FALSE;
			description = _("Not compatible with Wayland");
		}
#endif

		applet = g_new0 (PanelAddtoItemInfo, 1);
		applet->type = PANEL_ADDTO_APPLET;
		applet->name = g_strdup (name);
		applet->description = g_strdup (description);
		applet->icon = g_strdup (icon);
		applet->iid = g_strdup (iid);
		applet->enabled = enabled;
		applet->static_data = FALSE;

		list = g_slist_prepend (list, applet);
	}

	g_list_free (applet_list);

	return list;
}

static void
panel_addto_append_item (PanelAddtoDialog *dialog,
			 CtkListStore *model,
			 PanelAddtoItemInfo *applet)
{
	char *text;
	CtkTreeIter iter;

	if (applet == NULL) {
		ctk_list_store_append (model, &iter);
		ctk_list_store_set (model, &iter,
				    COLUMN_ICON_NAME, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    COLUMN_ENABLED, TRUE,
				    -1);
	} else {
		ctk_list_store_append (model, &iter);

		text = panel_addto_make_text (applet->name,
					      applet->description);

		ctk_list_store_set (model, &iter,
				    COLUMN_ICON_NAME, applet->icon,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, applet,
				    COLUMN_SEARCH, applet->name,
				    COLUMN_ENABLED, applet->enabled,
				    -1);

		g_free (text);
	}
}

static void
panel_addto_append_special_applets (PanelAddtoDialog *dialog,
				    CtkListStore *model)
{
	static gboolean translated = FALSE;
	int i;

	for (i = 0; i < G_N_ELEMENTS (special_addto_items); i++) {
		if (!translated) {
			special_addto_items [i].name = _(special_addto_items [i].name);
			special_addto_items [i].description = _(special_addto_items [i].description);
		}

		if (special_addto_items [i].type == PANEL_ADDTO_LAUNCHER_NEW
		    && panel_lockdown_get_disable_command_line ())
			continue;

		panel_addto_append_item (dialog, model, &special_addto_items [i]);
	}

	translated = TRUE;
}

static void
panel_addto_make_applet_model (PanelAddtoDialog *dialog)
{
	CtkListStore *model;
	GSList       *l;

	if (dialog->filter_applet_model != NULL)
		return;

	if (panel_profile_id_lists_are_writable ()) {
		dialog->applet_list = panel_addto_query_applets (dialog->applet_list);
		dialog->applet_list = panel_addto_prepend_internal_applets (dialog->applet_list);
	}

	dialog->applet_list = g_slist_sort (dialog->applet_list,
					    (GCompareFunc) panel_addto_applet_info_sort_func);

	model = ctk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING,
				    G_TYPE_BOOLEAN);

	if (panel_profile_id_lists_are_writable ()) {
		panel_addto_append_special_applets (dialog, model);
		if (dialog->applet_list)
			panel_addto_append_item (dialog, model, NULL);
	}

	for (l = dialog->applet_list; l; l = l->next)
		panel_addto_append_item (dialog, model, l->data);

	dialog->applet_model = CTK_TREE_MODEL (model);
	dialog->filter_applet_model = ctk_tree_model_filter_new (CTK_TREE_MODEL (dialog->applet_model),
								 NULL);
	ctk_tree_model_filter_set_visible_func (CTK_TREE_MODEL_FILTER (dialog->filter_applet_model),
						panel_addto_filter_func,
						dialog, NULL);
}

static void panel_addto_make_application_list (GSList             **parent_list,
					       CafeMenuTreeDirectory  *directory,
					       const char          *filename);

static void
panel_addto_prepend_directory (GSList             **parent_list,
			       CafeMenuTreeDirectory  *directory,
			       const char          *filename)
{
	PanelAddtoAppList *data;
	GIcon              *gicon;

	data = g_new0 (PanelAddtoAppList, 1);
	gicon = cafemenu_tree_directory_get_icon (directory);

	data->item_info.type          = PANEL_ADDTO_MENU;
	data->item_info.name          = g_strdup (cafemenu_tree_directory_get_name (directory));
	data->item_info.description   = g_strdup (cafemenu_tree_directory_get_comment (directory));
	data->item_info.icon          = gicon ? g_icon_to_string(gicon) : g_strdup(PANEL_ICON_UNKNOWN);
	data->item_info.menu_filename = g_strdup (filename);
	data->item_info.menu_path     = cafemenu_tree_directory_make_path (directory, NULL);
	data->item_info.enabled       = TRUE;
	data->item_info.static_data   = FALSE;

	/* We should set the iid here to something and do
	 * iid = g_strdup_printf ("MENU:%s", tfr->name)
	 * but this means we'd have to free the iid later
	 * and this would complexify too much the free
	 * function.
	 * So the iid is built when we select the row.
	 */

	*parent_list = g_slist_prepend (*parent_list, data);

	panel_addto_make_application_list (&data->children, directory, filename);
}

static void
panel_addto_prepend_entry (GSList         **parent_list,
			   CafeMenuTreeEntry  *entry,
			   const char      *filename)
{
	PanelAddtoAppList *data;
	GDesktopAppInfo    *ginfo;
	GIcon              *gicon;

	ginfo = cafemenu_tree_entry_get_app_info (entry);
	gicon = g_app_info_get_icon(G_APP_INFO(ginfo));

	data = g_new0 (PanelAddtoAppList, 1);

	data->item_info.type          = PANEL_ADDTO_LAUNCHER;
	data->item_info.name          = g_strdup (g_app_info_get_display_name(G_APP_INFO(ginfo)));
	data->item_info.description   = g_strdup (g_app_info_get_description(G_APP_INFO(ginfo)));
	data->item_info.icon          = gicon ? g_icon_to_string(gicon) : g_strdup(PANEL_ICON_UNKNOWN);
	data->item_info.launcher_path = g_strdup (cafemenu_tree_entry_get_desktop_file_path (entry));
	data->item_info.enabled       = TRUE;
	data->item_info.static_data   = FALSE;

	*parent_list = g_slist_prepend (*parent_list, data);
}

static void
panel_addto_prepend_alias (GSList         **parent_list,
			   CafeMenuTreeAlias  *alias,
			   const char      *filename)
{
	gpointer item;

	switch (cafemenu_tree_alias_get_aliased_item_type (alias)) {
	case CAFEMENU_TREE_ITEM_DIRECTORY:
		item = cafemenu_tree_alias_get_directory(alias);
		panel_addto_prepend_directory (parent_list,
				item,
				filename);
		cafemenu_tree_item_unref (item);
		break;

	case CAFEMENU_TREE_ITEM_ENTRY:
		item = cafemenu_tree_alias_get_aliased_entry(alias);
		panel_addto_prepend_entry (parent_list,
				item,
				filename);
		cafemenu_tree_item_unref (item);
		break;

	default:
		break;
	}
}

static void
panel_addto_make_application_list (GSList             **parent_list,
				   CafeMenuTreeDirectory  *directory,
				   const char          *filename)
{
	CafeMenuTreeIter *iter;
	iter = cafemenu_tree_directory_iter (directory);
	CafeMenuTreeItemType type;
	while ((type = cafemenu_tree_iter_next (iter)) != CAFEMENU_TREE_ITEM_INVALID) {
		gpointer item;
		switch (type) {
		case CAFEMENU_TREE_ITEM_DIRECTORY:
			item = cafemenu_tree_iter_get_directory(iter);
			panel_addto_prepend_directory (parent_list, item, filename);
			cafemenu_tree_item_unref (item);
			break;

		case CAFEMENU_TREE_ITEM_ENTRY:
			item = cafemenu_tree_iter_get_entry (iter);
			panel_addto_prepend_entry (parent_list, item, filename);
			cafemenu_tree_item_unref (item);
			break;

		case CAFEMENU_TREE_ITEM_ALIAS:
			item = cafemenu_tree_iter_get_alias(iter);
			panel_addto_prepend_alias (parent_list, item, filename);
			cafemenu_tree_item_unref (item);
			break;
		default:
			break;
		}
	}
	cafemenu_tree_iter_unref (iter);

	*parent_list = g_slist_reverse (*parent_list);
}

static void
panel_addto_populate_application_model (CtkTreeStore *store,
					CtkTreeIter  *parent,
					GSList       *app_list)
{
	PanelAddtoAppList *data;
	CtkTreeIter        iter;
	char              *text;
	GSList            *app;

	for (app = app_list; app != NULL; app = app->next) {
		data = app->data;
		ctk_tree_store_append (store, &iter, parent);

		text = panel_addto_make_text (data->item_info.name,
					      data->item_info.description);
		ctk_tree_store_set (store, &iter,
				    COLUMN_ICON_NAME, data->item_info.icon,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, &(data->item_info),
				    COLUMN_SEARCH, data->item_info.name,
				    COLUMN_ENABLED, data->item_info.enabled,
				    -1);

		g_free (text);

		if (data->children != NULL)
			panel_addto_populate_application_model (store,
								&iter,
								data->children);
	}
}

static void panel_addto_make_application_model(PanelAddtoDialog* dialog)
{
	CtkTreeStore* store;
	CafeMenuTree* tree;
	CafeMenuTreeDirectory* root;
	GError *error = NULL;

	if (dialog->filter_application_model != NULL)
		return;

	store = ctk_tree_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING,
				    G_TYPE_BOOLEAN);

	tree = cafemenu_tree_new ("cafe-applications.menu", CAFEMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
	if (! cafemenu_tree_load_sync (tree, &error)) {
		g_warning("Applications menu tree loading got error:%s\n", error->message);
		g_error_free(error);
		g_clear_object(&tree);
	}

	if ((root = cafemenu_tree_get_root_directory (tree)) != NULL )
	{
		panel_addto_make_application_list(&dialog->application_list, root, "cafe-applications.menu");
		panel_addto_populate_application_model(store, NULL, dialog->application_list);

		cafemenu_tree_item_unref(root);
	}

	g_clear_object(&tree);

	tree = cafemenu_tree_new ("cafe-settings.menu", CAFEMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
	if (! cafemenu_tree_load_sync (tree, &error)) {
		g_warning("Settings menu tree loading got error:%s\n", error->message);
		g_error_free(error);
		g_clear_object(&tree);
	}

	if ((root = cafemenu_tree_get_root_directory(tree)))
	{
		CtkTreeIter iter;

		ctk_tree_store_append(store, &iter, NULL);
		ctk_tree_store_set (store, &iter,
				    COLUMN_ICON_NAME, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    COLUMN_ENABLED, TRUE,
				    -1);

		panel_addto_make_application_list(&dialog->settings_list, root, "cafe-settings.menu");
		panel_addto_populate_application_model(store, NULL, dialog->settings_list);

		cafemenu_tree_item_unref(root);
	}

	g_object_unref(tree);

	dialog->application_model = CTK_TREE_MODEL(store);
	dialog->filter_application_model = ctk_tree_model_filter_new(CTK_TREE_MODEL(dialog->application_model), NULL);
	ctk_tree_model_filter_set_visible_func(CTK_TREE_MODEL_FILTER(dialog->filter_application_model), panel_addto_filter_func, dialog, NULL);
}

static void
panel_addto_add_item (PanelAddtoDialog   *dialog,
	 	      PanelAddtoItemInfo *item_info)
{
	g_assert (item_info != NULL);

	switch (item_info->type) {
	case PANEL_ADDTO_APPLET:
		cafe_panel_applet_frame_create (dialog->panel_widget->toplevel,
					   dialog->insertion_position,
					   item_info->iid);
		break;
	case PANEL_ADDTO_ACTION:
		panel_action_button_create (dialog->panel_widget->toplevel,
					    dialog->insertion_position,
					    item_info->action_type);
		break;
	case PANEL_ADDTO_LAUNCHER_MENU:
		panel_addto_present_applications (dialog);
		break;
	case PANEL_ADDTO_LAUNCHER:
		panel_launcher_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position,
				       item_info->launcher_path);
		break;
	case PANEL_ADDTO_LAUNCHER_NEW:
		ask_about_launcher (NULL, dialog->panel_widget,
				    dialog->insertion_position, FALSE);
		break;
	case PANEL_ADDTO_MENU:
		panel_menu_button_create (dialog->panel_widget->toplevel,
					  dialog->insertion_position,
					  item_info->menu_filename,
					  item_info->menu_path,
					  item_info->menu_path != NULL,
					  item_info->name);
		break;
	case PANEL_ADDTO_MENUBAR:
		panel_menu_bar_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position);
		break;
	case PANEL_ADDTO_SEPARATOR:
		panel_separator_create (dialog->panel_widget->toplevel,
					dialog->insertion_position);
		break;
	case PANEL_ADDTO_DRAWER:
		panel_drawer_create (dialog->panel_widget->toplevel,
				     dialog->insertion_position,
				     NULL, FALSE, NULL);
		break;
	}
}

static void
panel_addto_dialog_response (CtkWidget *widget_dialog,
			     guint response_id,
			     PanelAddtoDialog *dialog)
{
	CtkTreeSelection *selection;
	CtkTreeModel     *filter_model;
	CtkTreeModel     *child_model;
	CtkTreeIter       iter;
	CtkTreeIter       filter_iter;

	switch (response_id) {
	case CTK_RESPONSE_HELP:
		panel_show_help (ctk_window_get_screen (CTK_WINDOW (dialog->addto_dialog)),
				 "cafe-user-guide", "gospanel-15", NULL);
		break;

	case PANEL_ADDTO_RESPONSE_ADD:
		selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->tree_view));
		if (ctk_tree_selection_get_selected (selection, &filter_model,
						     &filter_iter)) {
			PanelAddtoItemInfo *data;

			ctk_tree_model_filter_convert_iter_to_child_iter (CTK_TREE_MODEL_FILTER (filter_model),
									  &iter,
									  &filter_iter);
			child_model = ctk_tree_model_filter_get_model (CTK_TREE_MODEL_FILTER (filter_model));
			ctk_tree_model_get (child_model, &iter,
					    COLUMN_DATA, &data, -1);

			if (data != NULL)
				panel_addto_add_item (dialog, data);
		}
		break;

	case PANEL_ADDTO_RESPONSE_BACK:
		/* This response only happens when we're showing the
		 * application list and the user wants to go back to the
		 * applet list. */
		panel_addto_present_applets (dialog);
		break;

	case CTK_RESPONSE_CLOSE:
		ctk_widget_destroy (widget_dialog);
		break;

	default:
		break;
	}
}

static void
panel_addto_dialog_destroy (CtkWidget *widget_dialog,
			    PanelAddtoDialog *dialog)
{
	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (dialog->panel_widget->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->panel_widget->toplevel),
			    panel_addto_dialog_quark,
			    NULL);
}

static void
panel_addto_present_applications (PanelAddtoDialog *dialog)
{
	if (dialog->filter_application_model == NULL)
		panel_addto_make_application_model (dialog);
	ctk_tree_view_set_model (CTK_TREE_VIEW (dialog->tree_view),
				 dialog->filter_application_model);
	ctk_window_set_focus (CTK_WINDOW (dialog->addto_dialog),
			      dialog->search_entry);
	ctk_widget_set_sensitive (dialog->back_button, TRUE);

	if (dialog->applet_search_text)
		g_free (dialog->applet_search_text);

	dialog->applet_search_text = g_strdup (ctk_entry_get_text (CTK_ENTRY (dialog->search_entry)));
	/* show everything */
	ctk_entry_set_text (CTK_ENTRY (dialog->search_entry), "");
}

static void
panel_addto_present_applets (PanelAddtoDialog *dialog)
{
	if (dialog->filter_applet_model == NULL)
		panel_addto_make_applet_model (dialog);
	ctk_tree_view_set_model (CTK_TREE_VIEW (dialog->tree_view),
				 dialog->filter_applet_model);
	ctk_window_set_focus (CTK_WINDOW (dialog->addto_dialog),
			      dialog->search_entry);
	ctk_widget_set_sensitive (dialog->back_button, FALSE);

	if (dialog->applet_search_text) {
		ctk_entry_set_text (CTK_ENTRY (dialog->search_entry),
				    dialog->applet_search_text);
		ctk_editable_set_position (CTK_EDITABLE (dialog->search_entry),
					   -1);

		g_free (dialog->applet_search_text);
		dialog->applet_search_text = NULL;
	}
}

static void
panel_addto_dialog_free_item_info (PanelAddtoItemInfo *item_info)
{
	if (item_info == NULL || item_info->static_data)
		return;

	if (item_info->name != NULL)
		g_free (item_info->name);
	item_info->name = NULL;

	if (item_info->description != NULL)
		g_free (item_info->description);
	item_info->description = NULL;

	if (item_info->icon != NULL)
		g_free (item_info->icon);
	item_info->icon = NULL;

	if (item_info->iid != NULL)
		g_free (item_info->iid);
	item_info->iid = NULL;

	if (item_info->launcher_path != NULL)
		g_free (item_info->launcher_path);
	item_info->launcher_path = NULL;

	if (item_info->menu_filename != NULL)
		g_free (item_info->menu_filename);
	item_info->menu_filename = NULL;

	if (item_info->menu_path != NULL)
		g_free (item_info->menu_path);
	item_info->menu_path = NULL;
}

static void
panel_addto_dialog_free_application_list (GSList *application_list)
{
	PanelAddtoAppList *data;
	GSList            *app;

	if (application_list == NULL)
		return;

	for (app = application_list; app != NULL; app = app->next) {
		data = app->data;
		if (data->children) {
			panel_addto_dialog_free_application_list (data->children);
		}
		panel_addto_dialog_free_item_info (&data->item_info);
		g_free (data);
	}
	g_slist_free (application_list);
}

static void
panel_addto_name_notify (GSettings        *settings,
			 gchar            *key,
			 PanelAddtoDialog *dialog);

static void
panel_addto_dialog_free (PanelAddtoDialog *dialog)
{
	GSList      *item;

	g_signal_handlers_disconnect_by_func(dialog->panel_widget->toplevel->settings,
					     G_CALLBACK (panel_addto_name_notify),
					     dialog);

	if (dialog->search_text)
		g_free (dialog->search_text);
	dialog->search_text = NULL;

	if (dialog->applet_search_text)
		g_free (dialog->applet_search_text);
	dialog->applet_search_text = NULL;

	if (dialog->addto_dialog)
		ctk_widget_destroy (dialog->addto_dialog);
	dialog->addto_dialog = NULL;

	for (item = dialog->applet_list; item != NULL; item = item->next) {
		PanelAddtoItemInfo *applet;

		applet = (PanelAddtoItemInfo *) item->data;
		if (!applet->static_data) {
			panel_addto_dialog_free_item_info (applet);
			g_free (applet);
		}
	}
	g_slist_free (dialog->applet_list);

	panel_addto_dialog_free_application_list (dialog->application_list);
	panel_addto_dialog_free_application_list (dialog->settings_list);

	if (dialog->filter_applet_model)
		g_object_unref (dialog->filter_applet_model);
	dialog->filter_applet_model = NULL;

	if (dialog->applet_model)
		g_object_unref (dialog->applet_model);
	dialog->applet_model = NULL;

	if (dialog->filter_application_model)
		g_object_unref (dialog->filter_application_model);
	dialog->filter_application_model = NULL;

	if (dialog->application_model)
		g_object_unref (dialog->application_model);
	dialog->application_model = NULL;

	g_clear_object (&dialog->menu_tree);

	g_free (dialog);
}

static void
panel_addto_name_change (PanelAddtoDialog *dialog,
			 const char       *name)
{
	char *title;
	char *label;

	label = NULL;

	if (!PANEL_GLIB_STR_EMPTY (name))
		label = g_strdup_printf (_("Find an _item to add to \"%s\":"),
					 name);

	if (panel_toplevel_get_is_attached (dialog->panel_widget->toplevel)) {
		title = g_strdup_printf (_("Add to Drawer"));
		if (label == NULL)
			label = g_strdup (_("Find an _item to add to the drawer:"));
	} else {
		title = g_strdup_printf (_("Add to Panel"));
		if (label == NULL)
			label = g_strdup (_("Find an _item to add to the panel:"));
	}

	ctk_window_set_title (CTK_WINDOW (dialog->addto_dialog), title);
	g_free (title);

	ctk_label_set_text_with_mnemonic (CTK_LABEL (dialog->label), label);
	g_free (label);
}

static void
panel_addto_name_notify (GSettings        *settings,
			 gchar            *key,
			 PanelAddtoDialog *dialog)
{
	gchar *name = g_settings_get_string (settings, key);
	panel_addto_name_change (dialog, name);
	g_free (name);
}

static gboolean
panel_addto_filter_func (CtkTreeModel *model,
			 CtkTreeIter  *iter,
			 gpointer      userdata)
{
	PanelAddtoDialog   *dialog;
	PanelAddtoItemInfo *data;

	dialog = (PanelAddtoDialog *) userdata;

	if (!dialog->search_text || !dialog->search_text[0])
		return TRUE;

	ctk_tree_model_get (model, iter, COLUMN_DATA, &data, -1);

	if (data == NULL)
		return FALSE;

	/* This is more a workaround than anything else: show all the root
	 * items in a tree store */
	if (CTK_IS_TREE_STORE (model) &&
	    ctk_tree_store_iter_depth (CTK_TREE_STORE (model), iter) == 0)
		return TRUE;

	return (panel_g_utf8_strstrcase (data->name,
					 dialog->search_text) != NULL ||
	        panel_g_utf8_strstrcase (data->description,
					 dialog->search_text) != NULL);
}

static void
panel_addto_search_entry_changed (CtkWidget        *entry,
				  PanelAddtoDialog *dialog)
{
	CtkTreeModel *model;
	char         *new_text;
	CtkTreeIter   iter;
	CtkTreePath  *path;

	new_text = g_strdup (ctk_entry_get_text (CTK_ENTRY (dialog->search_entry)));
	g_strchomp (new_text);

	if (dialog->search_text &&
	    g_utf8_collate (new_text, dialog->search_text) == 0) {
		g_free (new_text);
		return;
	}

	if (dialog->search_text)
		g_free (dialog->search_text);
	dialog->search_text = new_text;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (dialog->tree_view));
	ctk_tree_model_filter_refilter (CTK_TREE_MODEL_FILTER (model));

	path = ctk_tree_path_new_first ();
	if (ctk_tree_model_get_iter (model, &iter, path)) {
		CtkTreeSelection *selection;

		ctk_tree_view_scroll_to_cell (CTK_TREE_VIEW (dialog->tree_view),
					      path, NULL, FALSE, 0, 0);
		selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->tree_view));
		ctk_tree_selection_select_path (selection, path);
	}
	ctk_tree_path_free (path);
}

static void
panel_addto_search_entry_activated (CtkWidget        *entry,
				    PanelAddtoDialog *dialog)
{
	ctk_dialog_response (CTK_DIALOG (dialog->addto_dialog),
			     PANEL_ADDTO_RESPONSE_ADD);
}

static gboolean
panel_addto_selection_func (CtkTreeSelection  *selection,
			    CtkTreeModel      *model,
			    CtkTreePath       *path,
			    gboolean           path_currently_selected,
			    gpointer           data)
{
	CtkTreeIter         iter;
	gboolean            enabled;

	if (!ctk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	ctk_tree_model_get (model, &iter,
			    COLUMN_ENABLED, &enabled,
			    -1);
	return enabled;
}

static void
panel_addto_selection_changed (CtkTreeSelection *selection,
			       PanelAddtoDialog *dialog)
{
	CtkTreeModel       *filter_model;
	CtkTreeModel       *child_model;
	CtkTreeIter         iter;
	CtkTreeIter         filter_iter;
	PanelAddtoItemInfo *data;
	char               *iid;

	if (!ctk_tree_selection_get_selected (selection,
					      &filter_model,
					      &filter_iter)) {
		ctk_widget_set_sensitive (CTK_WIDGET (dialog->add_button),
					  FALSE);
		return;
	}

	ctk_tree_model_filter_convert_iter_to_child_iter (CTK_TREE_MODEL_FILTER (filter_model),
							  &iter, &filter_iter);
	child_model = ctk_tree_model_filter_get_model (CTK_TREE_MODEL_FILTER (filter_model));
	ctk_tree_model_get (child_model, &iter, COLUMN_DATA, &data, -1);

	if (!data) {
		ctk_widget_set_sensitive (CTK_WIDGET (dialog->add_button),
					  FALSE);
		return;
	}

	ctk_widget_set_sensitive (CTK_WIDGET (dialog->add_button), TRUE);

	if (data->type == PANEL_ADDTO_LAUNCHER_MENU) {
		ctk_button_set_image (CTK_BUTTON (dialog->add_button),
				      ctk_image_new_from_icon_name ("go-next", CTK_ICON_SIZE_BUTTON));
	} else {
		ctk_button_set_image (CTK_BUTTON (dialog->add_button),
				      ctk_image_new_from_icon_name ("list-add", CTK_ICON_SIZE_BUTTON));
	}

	/* only allow dragging applets if we can add applets */
	if (panel_profile_id_lists_are_writable ()) {
		switch (data->type) {
		case PANEL_ADDTO_LAUNCHER:
			panel_addto_setup_launcher_drag (CTK_TREE_VIEW (dialog->tree_view),
							 data->launcher_path);
			break;
		case PANEL_ADDTO_APPLET:
			panel_addto_setup_applet_drag (CTK_TREE_VIEW (dialog->tree_view),
						       data->iid);
			break;
		case PANEL_ADDTO_LAUNCHER_MENU:
			ctk_tree_view_unset_rows_drag_source (CTK_TREE_VIEW (dialog->tree_view));
			break;
		case PANEL_ADDTO_MENU:
			/* build the iid for menus other than the main menu */
			if (data->iid == NULL) {
				iid = g_strdup_printf ("MENU:%s/%s",
						       data->menu_filename,
						       data->menu_path);
			} else {
				iid = g_strdup (data->iid);
			}
			panel_addto_setup_internal_applet_drag (CTK_TREE_VIEW (dialog->tree_view),
							        iid);
			g_free (iid);
			break;
		default:
			panel_addto_setup_internal_applet_drag (CTK_TREE_VIEW (dialog->tree_view),
							        data->iid);
			break;
		}
	}
}

static void
panel_addto_selection_activated (CtkTreeView       *view,
				 CtkTreePath       *path,
				 CtkTreeViewColumn *column,
				 PanelAddtoDialog  *dialog)
{
	ctk_dialog_response (CTK_DIALOG (dialog->addto_dialog),
			     PANEL_ADDTO_RESPONSE_ADD);
}

static gboolean
panel_addto_separator_func (CtkTreeModel *model,
			    CtkTreeIter *iter,
			    gpointer data)
{
	int column = GPOINTER_TO_INT (data);
	char *text;

	ctk_tree_model_get (model, iter, column, &text, -1);

	if (!text)
		return TRUE;

	g_free(text);
	return FALSE;
}

static PanelAddtoDialog *
panel_addto_dialog_new (PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	CtkWidget *dialog_vbox;
	CtkWidget *inner_vbox;
	CtkWidget *find_hbox;
	CtkWidget *sw;
	CtkCellRenderer *renderer;
	CtkTreeSelection *selection;
	CtkTreeViewColumn *column;

	dialog = g_new0 (PanelAddtoDialog, 1);

	g_object_set_qdata_full (G_OBJECT (panel_widget->toplevel),
				 panel_addto_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_addto_dialog_free);

	dialog->panel_widget = panel_widget;

	g_signal_connect (dialog->panel_widget->toplevel->settings,
			  "changed::" PANEL_TOPLEVEL_NAME_KEY,
			  G_CALLBACK (panel_addto_name_notify),
			  dialog);

	dialog->addto_dialog = ctk_dialog_new ();

	panel_dialog_add_button (CTK_DIALOG (dialog->addto_dialog),
				 _("_Help"), "help-browser", CTK_RESPONSE_HELP);

	dialog->back_button = panel_dialog_add_button (CTK_DIALOG (dialog->addto_dialog),
						       _("_Back"), "go-previous",
						       PANEL_ADDTO_RESPONSE_BACK);

	dialog->add_button = panel_dialog_add_button (CTK_DIALOG (dialog->addto_dialog),
						      _("_Add"), "list-add",
						      PANEL_ADDTO_RESPONSE_ADD);

	panel_dialog_add_button (CTK_DIALOG (dialog->addto_dialog),
				 _("_Close"), "window-close",
				 CTK_RESPONSE_CLOSE);

	ctk_widget_set_sensitive (CTK_WIDGET (dialog->add_button), FALSE);

	ctk_dialog_set_default_response (CTK_DIALOG (dialog->addto_dialog),
					 PANEL_ADDTO_RESPONSE_ADD);

	ctk_container_set_border_width (CTK_CONTAINER (dialog->addto_dialog), 5);

	dialog_vbox = ctk_dialog_get_content_area (CTK_DIALOG (dialog->addto_dialog));
	ctk_box_set_spacing (CTK_BOX (dialog_vbox), 12);
	ctk_container_set_border_width (CTK_CONTAINER (dialog_vbox), 5);

	g_signal_connect (G_OBJECT (dialog->addto_dialog), "response",
			  G_CALLBACK (panel_addto_dialog_response), dialog);
	g_signal_connect (dialog->addto_dialog, "destroy",
			  G_CALLBACK (panel_addto_dialog_destroy), dialog);

	inner_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_box_pack_start (CTK_BOX (dialog_vbox), inner_vbox, TRUE, TRUE, 0);

	find_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_box_pack_start (CTK_BOX (inner_vbox), find_hbox, FALSE, FALSE, 0);

	dialog->label = ctk_label_new_with_mnemonic ("");
	ctk_label_set_xalign (CTK_LABEL (dialog->label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (dialog->label), 0.5);
	ctk_label_set_use_markup (CTK_LABEL (dialog->label), TRUE);

	ctk_box_pack_start (CTK_BOX (find_hbox), dialog->label,
			    FALSE, FALSE, 0);

	dialog->search_entry = ctk_entry_new ();
	g_signal_connect (G_OBJECT (dialog->search_entry), "changed",
			  G_CALLBACK (panel_addto_search_entry_changed), dialog);
	g_signal_connect (G_OBJECT (dialog->search_entry), "activate",
			  G_CALLBACK (panel_addto_search_entry_activated), dialog);

	ctk_box_pack_end (CTK_BOX (find_hbox), dialog->search_entry,
			  TRUE, TRUE, 0);

	ctk_label_set_mnemonic_widget (CTK_LABEL (dialog->label),
				       dialog->search_entry);

	sw = ctk_scrolled_window_new (NULL, NULL);
	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
					CTK_POLICY_AUTOMATIC,
					CTK_POLICY_AUTOMATIC);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (sw),
					     CTK_SHADOW_IN);
	ctk_box_pack_start (CTK_BOX (inner_vbox), sw, TRUE, TRUE, 0);

	dialog->tree_view = ctk_tree_view_new ();
	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (dialog->tree_view),
					   FALSE);
	ctk_tree_view_expand_all (CTK_TREE_VIEW (dialog->tree_view));

	renderer = g_object_new (CTK_TYPE_CELL_RENDERER_PIXBUF,
				 "xpad", 4,
				 "ypad", 4,
				 "stock-size", panel_add_to_icon_get_size(),
				 NULL);

	ctk_tree_view_insert_column_with_attributes (CTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     renderer,
						     "icon_name", COLUMN_ICON_NAME,
						     "sensitive", COLUMN_ENABLED,
						     NULL);
	renderer = ctk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	ctk_tree_view_insert_column_with_attributes (CTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     renderer,
						     "markup", COLUMN_TEXT,
						     "sensitive", COLUMN_ENABLED,
						     NULL);

	//FIXME use the same search than the one for the search entry?
	ctk_tree_view_set_search_column (CTK_TREE_VIEW (dialog->tree_view),
					 COLUMN_SEARCH);

	ctk_tree_view_set_row_separator_func (CTK_TREE_VIEW (dialog->tree_view),
					      panel_addto_separator_func,
					      GINT_TO_POINTER (COLUMN_TEXT),
					      NULL);


	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->tree_view));
	ctk_tree_selection_set_mode (selection, CTK_SELECTION_SINGLE);

	column = ctk_tree_view_get_column (CTK_TREE_VIEW (dialog->tree_view),
					   COLUMN_TEXT);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_FIXED);

	ctk_tree_selection_set_select_function (selection, panel_addto_selection_func, NULL, NULL);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (panel_addto_selection_changed),
			  dialog);

	g_signal_connect (dialog->tree_view, "row-activated",
			  G_CALLBACK (panel_addto_selection_activated),
			  dialog);

	ctk_container_add (CTK_CONTAINER (sw), dialog->tree_view);

	ctk_widget_show_all (dialog_vbox);

	panel_toplevel_push_autohide_disabler (dialog->panel_widget->toplevel);
	panel_widget_register_open_dialog (panel_widget,
					   dialog->addto_dialog);

	panel_addto_name_change (dialog,
				 panel_toplevel_get_name (dialog->panel_widget->toplevel));

	return dialog;
}

#define MAX_ADDTOPANEL_HEIGHT 490

void
panel_addto_present (CtkMenuItem *item,
		     PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	PanelToplevel *toplevel;
	PanelData     *pd;
	GdkScreen *screen;
	GdkMonitor *monitor;
	GdkRectangle monitor_geom;
	gint height;

	toplevel = panel_widget->toplevel;
	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");

	if (!panel_addto_dialog_quark)
		panel_addto_dialog_quark =
			g_quark_from_static_string ("panel-addto-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel),
				     panel_addto_dialog_quark);

	screen = ctk_window_get_screen (CTK_WINDOW (toplevel));
	monitor = cdk_display_get_monitor_at_window (ctk_widget_get_display (CTK_WIDGET (toplevel)),
						     ctk_widget_get_window (CTK_WIDGET (toplevel)));
	cdk_monitor_get_geometry (monitor, &monitor_geom);
	height = MIN (MAX_ADDTOPANEL_HEIGHT, 3 * (monitor_geom.height / 4));

	if (!dialog) {
		dialog = panel_addto_dialog_new (panel_widget);
		panel_addto_present_applets (dialog);
	}

	dialog->insertion_position = pd ? pd->insertion_pos : -1;
	ctk_window_set_screen (CTK_WINDOW (dialog->addto_dialog), screen);
	ctk_window_set_default_size (CTK_WINDOW (dialog->addto_dialog),
				     height * 8 / 7, height);
	ctk_window_present (CTK_WINDOW (dialog->addto_dialog));
}
