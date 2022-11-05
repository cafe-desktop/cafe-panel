/* Cafe panel: general applet functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libpanel-util/panel-show.h>
#include <libpanel-util/panel-ctk.h>

#include "button-widget.h"
#include "drawer.h"
#include "launcher.h"
#include "panel-addto.h"
#include "panel-config-global.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-separator.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-profile.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-properties-dialog.h"
#include "panel-lockdown.h"
#include "panel-schemas.h"

#define SMALL_ICON_SIZE 20

static GSList *registered_applets = NULL;
static GSList *queued_position_saves = NULL;
static guint   queued_position_source = 0;

static GtkCheckMenuItem *checkbox_id = NULL;

static void applet_menu_show (GtkWidget *w, AppletInfo *info);
static void applet_menu_deactivate (GtkWidget *w, AppletInfo *info);

static inline PanelWidget *
cafe_panel_applet_get_panel_widget (AppletInfo *info)
{
	return PANEL_WIDGET (ctk_widget_get_parent (info->widget));
}

static void
cafe_panel_applet_set_dnd_enabled (AppletInfo *info,
			      gboolean    dnd_enabled)
{
	switch (info->type) {
	case PANEL_OBJECT_DRAWER:
		panel_drawer_set_dnd_enabled (info->data, dnd_enabled);
		break;
	case PANEL_OBJECT_MENU:
		panel_menu_button_set_dnd_enabled (PANEL_MENU_BUTTON (info->widget),
						   dnd_enabled);
		break;
	case PANEL_OBJECT_LAUNCHER:
		panel_launcher_set_dnd_enabled (info->data, dnd_enabled);
		break;
	case PANEL_OBJECT_APPLET:
		break;
	case PANEL_OBJECT_ACTION:
		panel_action_button_set_dnd_enabled (PANEL_ACTION_BUTTON (info->widget),
						     dnd_enabled);
		break;
	case PANEL_OBJECT_MENU_BAR:
	case PANEL_OBJECT_SEPARATOR:
		break;
	default:
		g_assert_not_reached ();
		break;
	}

}

gboolean
cafe_panel_applet_toggle_locked (AppletInfo *info)
{
	PanelWidget *panel_widget;
	gboolean     locked;

	panel_widget = cafe_panel_applet_get_panel_widget (info);

	locked = panel_widget_toggle_applet_locked (panel_widget, info->widget);

	cafe_panel_applet_save_position (info, info->id, TRUE);
	cafe_panel_applet_set_dnd_enabled (info, !locked);

	return locked;
}

static void
checkbox_status (GtkCheckMenuItem *menuitem,
		 AppletInfo       *info)
{
	checkbox_id = CTK_CHECK_MENU_ITEM (menuitem);
}

static void
cafe_panel_applet_lock (GtkMenuItem *menuitem,
			AppletInfo  *info)
{
	gboolean locked;

	locked = cafe_panel_applet_toggle_locked (info);

	ctk_check_menu_item_set_active (checkbox_id, locked);

	if (info->move_item)
		ctk_widget_set_sensitive (info->move_item, !locked);
}

static void
move_applet_callback (GtkWidget *widget, AppletInfo *info)
{
	GtkWidget   *parent;
	PanelWidget *panel;

	g_return_if_fail (info != NULL);
	g_return_if_fail (info->widget != NULL);

	parent = ctk_widget_get_parent (info->widget);

	g_return_if_fail (parent != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (parent));

	panel = PANEL_WIDGET (parent);

	panel_widget_applet_drag_start (panel, info->widget,
					PW_DRAG_OFF_CENTER,
					GDK_CURRENT_TIME);
}

/* permanently remove an applet - all non-permanent
 * cleanups should go in cafe_panel_applet_destroy()
 */
void
cafe_panel_applet_clean (AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	if (info->type == PANEL_OBJECT_LAUNCHER)
		panel_launcher_delete (info->data);

	if (info->widget) {
		GtkWidget *widget = info->widget;

		info->widget = NULL;
		ctk_widget_destroy (widget);
	}
}

static void
cafe_panel_applet_recreate_menu (AppletInfo	*info)
{
	GList *l;

	if (!info->menu)
		return;

	for (l = info->user_menu; l; l = l->next) {
		AppletUserMenu *menu = l->data;

		menu->menuitem =NULL;
		menu->submenu =NULL;
	}

	g_signal_handlers_disconnect_by_func (info->menu, G_CALLBACK (applet_menu_show), info);
	g_signal_handlers_disconnect_by_func (info->menu, G_CALLBACK (applet_menu_deactivate), info);

	g_object_unref (info->menu);
	info->menu = cafe_panel_applet_create_menu (info);
}

static void
cafe_panel_applet_locked_change_notify (GSettings *settings,
									    gchar *key,
									    GtkWidget   *applet)
{
	gboolean     locked;
	gboolean     applet_locked;
	AppletInfo  *info;
	PanelWidget *panel_widget;

	if (applet == NULL || !CTK_IS_WIDGET (applet))
		return;

	info = (AppletInfo  *) g_object_get_data (G_OBJECT (applet),
						  "applet_info");
	if (info == NULL)
		return;

	locked = g_settings_get_boolean (settings, key);

	panel_widget = cafe_panel_applet_get_panel_widget (info);
	applet_locked = panel_widget_get_applet_locked (panel_widget,
							info->widget);

	if ((locked && applet_locked) || !(locked || applet_locked))
		return;

	cafe_panel_applet_toggle_locked (info);

	if (info->type == PANEL_OBJECT_APPLET)
		cafe_panel_applet_frame_sync_menu_state (CAFE_PANEL_APPLET_FRAME (info->widget));
	else
		cafe_panel_applet_recreate_menu (info);
}

static void
applet_remove_callback (GtkWidget  *widget,
			AppletInfo *info)
{

	if (info->type == PANEL_OBJECT_DRAWER)
		drawer_query_deletion (info->data);
	else
		panel_profile_delete_object (info);
}

static inline GdkScreen *
applet_user_menu_get_screen (AppletUserMenu *menu)
{
	PanelWidget *panel_widget;

	panel_widget = cafe_panel_applet_get_panel_widget (menu->info);

	return ctk_window_get_screen (CTK_WINDOW (panel_widget->toplevel));
}

static void
applet_callback_callback (GtkWidget      *widget,
			  AppletUserMenu *menu)
{
	GdkScreen *screen;

	g_return_if_fail (menu->info != NULL);

	screen = applet_user_menu_get_screen (menu);

	switch (menu->info->type) {
	case PANEL_OBJECT_LAUNCHER:
		if (!strcmp (menu->name, "launch"))
			launcher_launch (menu->info->data, NULL);
		else if (!strcmp (menu->name, "properties"))
			launcher_properties (menu->info->data);
		else if (g_str_has_prefix (menu->name, "launch-action_")) {
			const gchar *action;
			action = menu->name + (sizeof("launch-action_") - 1);
			launcher_launch (menu->info->data, action);
		}
		break;
	case PANEL_OBJECT_DRAWER:
		if (strcmp (menu->name, "add") == 0) {
			Drawer *drawer = menu->info->data;

			panel_addto_present (CTK_MENU_ITEM (widget),
					     panel_toplevel_get_panel_widget (drawer->toplevel));
		} else if (strcmp (menu->name, "properties") == 0) {
			Drawer *drawer = menu->info->data;

			panel_properties_dialog_present (drawer->toplevel);
		} else if (strcmp (menu->name, "help") == 0) {
			panel_show_help (screen,
					 "cafe-user-guide", "gospanel-18", NULL);
		}
		break;
	case PANEL_OBJECT_MENU:
		panel_menu_button_invoke_menu (
			PANEL_MENU_BUTTON (menu->info->widget), menu->name);
		break;
	case PANEL_OBJECT_ACTION:
		panel_action_button_invoke_menu (
			PANEL_ACTION_BUTTON (menu->info->widget), menu->name);
		break;
	case PANEL_OBJECT_MENU_BAR:
		panel_menu_bar_invoke_menu (
			PANEL_MENU_BAR (menu->info->widget), menu->name);
		break;

	case PANEL_OBJECT_APPLET:
		/*
		 * Applet's menu's are handled differently
		 */
		break;
	case PANEL_OBJECT_SEPARATOR:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
applet_menu_show (GtkWidget *w,
		  AppletInfo *info)
{
	PanelWidget *panel_widget;

	panel_widget = cafe_panel_applet_get_panel_widget (info);

	panel_toplevel_push_autohide_disabler (panel_widget->toplevel);
}


static void
applet_menu_deactivate (GtkWidget *w,
			AppletInfo *info)
{
	PanelWidget *panel_widget;

	panel_widget = cafe_panel_applet_get_panel_widget (info);

	panel_toplevel_pop_autohide_disabler (panel_widget->toplevel);
}

AppletUserMenu *
cafe_panel_applet_get_callback (GList      *user_menu,
			   const char *name)
{
	GList *l;

	for (l = user_menu; l; l = l->next) {
		AppletUserMenu *menu = l->data;

		if (strcmp (menu->name, name) == 0)
			return menu;
	}

	return NULL;
}

void
cafe_panel_applet_add_callback (AppletInfo          *info,
			   const char          *callback_name,
			   const char          *icon_name,
			   const char          *menuitem_text,
			   CallbackEnabledFunc  is_enabled_func)
{
	AppletUserMenu *menu;

	g_return_if_fail (info != NULL);
	g_return_if_fail (cafe_panel_applet_get_callback (info->user_menu,
						     callback_name) == NULL);

	menu                  = g_new0 (AppletUserMenu, 1);
	menu->name            = g_strdup (callback_name);
	menu->gicon           = panel_gicon_from_icon_name (icon_name);
	menu->text            = g_strdup (menuitem_text);
	menu->is_enabled_func = is_enabled_func;
	menu->sensitive       = TRUE;
	menu->info            = info;
	menu->menuitem        = NULL;
	menu->submenu         = NULL;

	info->user_menu = g_list_append (info->user_menu, menu);

	cafe_panel_applet_recreate_menu (info);
}

void
cafe_panel_applet_clear_user_menu (AppletInfo *info)
{
	GList *l;

	for (l = info->user_menu; l != NULL; l = l->next) {
		AppletUserMenu *umenu = l->data;

		g_free (umenu->name);
		g_clear_object (&(umenu->gicon));
		g_free (umenu->text);
		g_free (umenu);
	}

	g_list_free (info->user_menu);
	info->user_menu = NULL;
}

static void
setup_an_item (AppletUserMenu *menu,
	       GtkWidget      *submenu,
	       int             is_submenu)
{
	menu->menuitem = panel_image_menu_item_new_from_gicon (menu->gicon, menu->text);

	ctk_widget_show (menu->menuitem);

	g_signal_connect (G_OBJECT (menu->menuitem), "destroy",
			  G_CALLBACK (ctk_widget_destroyed),
			  &menu->menuitem);

	if(submenu)
		ctk_menu_shell_append (CTK_MENU_SHELL (submenu), menu->menuitem);

	/*if an item not a submenu*/
	if (!is_submenu) {
		g_signal_connect (menu->menuitem, "activate",
				  G_CALLBACK (applet_callback_callback),
				  menu);
		g_signal_connect (submenu, "destroy",
				  G_CALLBACK (ctk_widget_destroyed),
				  &menu->submenu);
	/* if the item is a submenu and doesn't have it's menu
	   created yet*/
	} else if (!menu->submenu) {
		menu->submenu = ctk_menu_new ();
	}

	if(menu->submenu) {
		ctk_menu_item_set_submenu(CTK_MENU_ITEM(menu->menuitem),
					  menu->submenu);
		g_signal_connect (G_OBJECT (menu->submenu), "destroy",
				    G_CALLBACK (ctk_widget_destroyed),
				    &menu->submenu);
	}

	ctk_widget_set_sensitive(menu->menuitem,menu->sensitive);
}

static void
add_to_submenus (AppletInfo *info,
		 const char *path,
		 const char *name,
		 AppletUserMenu *menu,
		 GtkWidget *submenu,
		 GList *user_menu)
{
	char *n = g_strdup (name);
	char *p = strchr (n, '/');
	char *t;
	AppletUserMenu *s_menu;

	/*this is the last one*/
	if (p == NULL) {
		g_free (n);
		setup_an_item (menu, submenu, FALSE);
		return;
	}

	/*this is the last one and we are a submenu, we have already been
	  set up*/
	if(p==(n + strlen(n) - 1)) {
		g_free(n);
		return;
	}

	*p = '\0';
	p++;

	t = g_strconcat (path, n, "/", NULL);
	s_menu = cafe_panel_applet_get_callback (user_menu, t);
	/*the user did not give us this sub menu, whoops, will create an empty
	  one then*/
	if (s_menu == NULL) {
		s_menu = g_new0 (AppletUserMenu,1);
		s_menu->name = g_strdup (t);
		s_menu->gicon = NULL;
		s_menu->text = g_strdup (_("???"));
		s_menu->sensitive = TRUE;
		s_menu->info = info;
		s_menu->menuitem = NULL;
		s_menu->submenu = NULL;
		info->user_menu = g_list_append (info->user_menu,s_menu);
		user_menu = info->user_menu;
	}

	if (s_menu->submenu == NULL) {
		s_menu->submenu = ctk_menu_new ();
		/*a more elegant way to do this should be done
		  when I don't want to go to sleep */
		if (s_menu->menuitem != NULL) {
			ctk_widget_destroy (s_menu->menuitem);
			s_menu->menuitem = NULL;
		}
	}
	if (s_menu->menuitem == NULL)
		setup_an_item (s_menu, submenu, TRUE);

	add_to_submenus (info, t, p, menu, s_menu->submenu, user_menu);

	g_free(t);
	g_free(n);
}

GtkWidget *
cafe_panel_applet_create_menu (AppletInfo *info)
{
	GtkWidget   *menu;
	GtkWidget   *menuitem;
	GList       *l;
	PanelWidget *panel_widget;
	gboolean     added_anything = FALSE;

	panel_widget = cafe_panel_applet_get_panel_widget (info);

	menu = g_object_ref_sink (ctk_menu_new ());

	ctk_menu_set_reserve_toggle_size (CTK_MENU (menu), FALSE);

	/* connect the show & deactivate signal, so that we can "disallow" and
	 * "re-allow" autohide when the menu is shown/deactivated.
	 */
	g_signal_connect (menu, "show",
			  G_CALLBACK (applet_menu_show), info);
	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (applet_menu_deactivate), info);

	for (l = info->user_menu; l; l = l->next) {
		AppletUserMenu *user_menu = l->data;

		if (user_menu->is_enabled_func && !user_menu->is_enabled_func ())
			continue;

		add_to_submenus (info, "", user_menu->name, user_menu,
				 menu, info->user_menu);

		added_anything = TRUE;
	}

	if (!panel_lockdown_get_locked_down ()) {
		gboolean   locked;
		gboolean   lockable;
		gboolean   movable;
		gboolean   removable;

		lockable = cafe_panel_applet_lockable (info);
		movable = cafe_panel_applet_can_freely_move (info);
		removable = panel_profile_id_lists_are_writable ();

		locked = panel_widget_get_applet_locked (panel_widget, info->widget);

		if (added_anything) {
			menuitem = ctk_separator_menu_item_new ();
			ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
			ctk_widget_show (menuitem);
		}

		menuitem = panel_image_menu_item_new_from_icon ("list-remove", _("_Remove From Panel"));

		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (applet_remove_callback), info);
		ctk_widget_show (menuitem);
		ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
		ctk_widget_set_sensitive (menuitem, (!locked || lockable) && removable);

		menuitem = panel_image_menu_item_new_from_icon (NULL, _("_Move"));

		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (move_applet_callback), info);
		ctk_widget_show (menuitem);
		ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
		ctk_widget_set_sensitive (menuitem, !locked && movable);

		g_assert (info->move_item == NULL);

		info->move_item = menuitem;
		g_object_add_weak_pointer (G_OBJECT (menuitem),
					   (gpointer *) &info->move_item);

		menuitem = ctk_separator_menu_item_new ();
		ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
		ctk_widget_show (menuitem);

		menuitem = ctk_check_menu_item_new_with_mnemonic (_("Loc_k To Panel"));

		ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (menuitem),
						locked);

		g_signal_connect (menuitem, "map",
				  G_CALLBACK (checkbox_status), info);

		menuitem = panel_check_menu_item_new (menuitem);

		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (cafe_panel_applet_lock), info);

		ctk_widget_show (menuitem);

		ctk_menu_shell_append (CTK_MENU_SHELL (menu), menuitem);
		ctk_widget_set_sensitive (menuitem, lockable);

		added_anything = TRUE;
	}

	if ( ! added_anything) {
		g_object_unref (menu);
		return NULL;
	}

/* Set up theme and transparency support */
	GtkWidget *toplevel = ctk_widget_get_toplevel (menu);
/* Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	GdkScreen *screen = ctk_widget_get_screen(CTK_WIDGET(toplevel));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);
/* Set menu and it's toplevel window to follow panel theme */
	GtkStyleContext *context;
	context = ctk_widget_get_style_context (CTK_WIDGET(toplevel));
	ctk_style_context_add_class(context,"gnome-panel-menu-bar");
	ctk_style_context_add_class(context,"cafe-panel-menu-bar");

	return menu;
}

void
cafe_panel_applet_menu_set_recurse (GtkMenu     *menu,
			       const gchar *key,
			       gpointer     data)
{
	GList *children;
	GList *l;

	g_object_set_data (G_OBJECT (menu), key, data);

	children = ctk_container_get_children (CTK_CONTAINER (menu));

	for (l = children; l; l = l->next) {
		GtkWidget *submenu = ctk_menu_item_get_submenu (CTK_MENU_ITEM (l->data));

		if (submenu)
			cafe_panel_applet_menu_set_recurse (
				CTK_MENU (submenu), key, data);
	}

	g_list_free (children);
}

static void
applet_show_menu (AppletInfo     *info,
		  GdkEventButton *event)
{
	PanelWidget *panel_widget;

	g_return_if_fail (info != NULL);

	panel_widget = cafe_panel_applet_get_panel_widget (info);

	if (info->menu == NULL)
		info->menu = cafe_panel_applet_create_menu (info);

	if (info->menu == NULL)
		return;

	cafe_panel_applet_menu_set_recurse (CTK_MENU (info->menu),
				       "menu_panel",
				       panel_widget);

	ctk_menu_set_screen (CTK_MENU (info->menu),
			     ctk_window_get_screen (CTK_WINDOW (panel_widget->toplevel)));

	if (!ctk_widget_get_realized (info->menu))
		ctk_widget_show (info->menu);

	ctk_menu_popup_at_pointer (CTK_MENU (info->menu), NULL);
}

static gboolean
applet_do_popup_menu (GtkWidget      *widget,
		      GdkEventButton *event,
		      AppletInfo     *info)
{
	if (cafe_panel_applet_is_in_drag ())
		return FALSE;

	if (info->type == PANEL_OBJECT_APPLET)
		return FALSE;

	applet_show_menu (info, event);

	return TRUE;
}

static gboolean
applet_popup_menu (GtkWidget      *widget,
		   AppletInfo     *info)
{
	GdkEventButton event;

	event.button = 3;
	event.time = GDK_CURRENT_TIME;

	return applet_do_popup_menu (widget, &event, info);
}

static gboolean
applet_button_press (GtkWidget      *widget,
		     GdkEventButton *event,
		     AppletInfo     *info)
{
	gboolean     applet_locked;
	PanelWidget *panel_widget;

	panel_widget = cafe_panel_applet_get_panel_widget (info);
	applet_locked = panel_widget_get_applet_locked (panel_widget,
							info->widget);

	if (!applet_locked) cafe_panel_applet_set_dnd_enabled (info, TRUE);

	if (event->button == 3)
		return applet_do_popup_menu (widget, event, info);

	return FALSE;
}

static void
cafe_panel_applet_destroy (GtkWidget  *widget,
		      AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	g_signal_handlers_disconnect_by_data(info->settings,widget);

	info->widget = NULL;

	if (info->settings) {
		g_object_unref (info->settings);
		info->settings = NULL;
	}

	registered_applets = g_slist_remove (registered_applets, info);

	queued_position_saves =
		g_slist_remove (queued_position_saves, info);

	if (info->type == PANEL_OBJECT_DRAWER) {
		Drawer *drawer = info->data;

		if (drawer->toplevel) {
			PanelWidget *panel_widget;

			panel_widget = panel_toplevel_get_panel_widget (
							drawer->toplevel);
			panel_widget->master_widget = NULL;

			ctk_widget_destroy (CTK_WIDGET (drawer->toplevel));
			drawer->toplevel = NULL;
		}
	}

	if (info->type != PANEL_OBJECT_APPLET)
		panel_lockdown_notify_remove (G_CALLBACK (cafe_panel_applet_recreate_menu),
					      info);

	if (info->menu) {
		g_signal_handlers_disconnect_by_func (info->menu, G_CALLBACK (applet_menu_show), info);
		g_signal_handlers_disconnect_by_func (info->menu, G_CALLBACK (applet_menu_deactivate), info);
		g_object_unref (info->menu);
	}
	info->menu = NULL;

	if (info->data_destroy)
		info->data_destroy (info->data);
	info->data = NULL;

	cafe_panel_applet_clear_user_menu (info);

	g_free (info->id);
	info->id = NULL;

	g_free (info);
}

typedef struct {
	char            *id;
	PanelObjectType  type;
	char            *toplevel_id;
	int              position;
	guint            right_stick : 1;
	guint            locked : 1;
} CafePanelAppletToLoad;

/* Each time those lists get both empty,
 * cafe_panel_applet_queue_initial_unhide_toplevels() should be called */
static GSList  *cafe_panel_applets_to_load = NULL;
static GSList  *cafe_panel_applets_loading = NULL;
/* We have a timeout to always unhide toplevels after a delay, in case of some
 * blocking applet */
#define         UNHIDE_TOPLEVELS_TIMEOUT_SECONDS 5
static guint    cafe_panel_applet_unhide_toplevels_timeout = 0;

static gboolean cafe_panel_applet_have_load_idle = FALSE;

static void
free_applet_to_load (CafePanelAppletToLoad *applet)
{
	g_free (applet->id);
	applet->id = NULL;

	g_free (applet->toplevel_id);
	applet->toplevel_id = NULL;

	g_free (applet);
}

gboolean
cafe_panel_applet_on_load_queue (const char *id)
{
	GSList *li;
	for (li = cafe_panel_applets_to_load; li != NULL; li = li->next) {
		CafePanelAppletToLoad *applet = li->data;
		if (strcmp (applet->id, id) == 0)
			return TRUE;
	}
	for (li = cafe_panel_applets_loading; li != NULL; li = li->next) {
		CafePanelAppletToLoad *applet = li->data;
		if (strcmp (applet->id, id) == 0)
			return TRUE;
	}
	return FALSE;
}

/* This doesn't do anything if the initial unhide already happened */
static gboolean
cafe_panel_applet_queue_initial_unhide_toplevels (gpointer user_data)
{
	GSList *l;

	if (cafe_panel_applet_unhide_toplevels_timeout != 0) {
		g_source_remove (cafe_panel_applet_unhide_toplevels_timeout);
		cafe_panel_applet_unhide_toplevels_timeout = 0;
	}

	for (l = panel_toplevel_list_toplevels (); l != NULL; l = l->next)
		panel_toplevel_queue_initial_unhide ((PanelToplevel *) l->data);

	return FALSE;
}

void
cafe_panel_applet_stop_loading (const char *id)
{
	CafePanelAppletToLoad *applet;
	GSList *l;

	for (l = cafe_panel_applets_loading; l; l = l->next) {
		applet = l->data;

		if (strcmp (applet->id, id) == 0)
			break;
	}

	/* this can happen if we reload an applet after it crashed,
	 * for example */
	if (l != NULL) {
		cafe_panel_applets_loading = g_slist_delete_link (cafe_panel_applets_loading, l);
		free_applet_to_load (applet);
	}

	if (cafe_panel_applets_loading == NULL && cafe_panel_applets_to_load == NULL)
		cafe_panel_applet_queue_initial_unhide_toplevels (NULL);
}

static gboolean
cafe_panel_applet_load_idle_handler (gpointer dummy)
{
	PanelObjectType    applet_type;
	CafePanelAppletToLoad *applet = NULL;
	PanelToplevel     *toplevel = NULL;
	PanelWidget       *panel_widget;
	GSList            *l;

	if (!cafe_panel_applets_to_load) {
		cafe_panel_applet_have_load_idle = FALSE;
		return FALSE;
	}

	for (l = cafe_panel_applets_to_load; l; l = l->next) {
		applet = l->data;

		toplevel = panel_profile_get_toplevel_by_id (applet->toplevel_id);
		if (toplevel)
			break;
	}

	if (!l) {
		/* All the remaining applets don't have a panel */
		for (l = cafe_panel_applets_to_load; l; l = l->next)
			free_applet_to_load (l->data);
		g_slist_free (cafe_panel_applets_to_load);
		cafe_panel_applets_to_load = NULL;
		cafe_panel_applet_have_load_idle = FALSE;

		if (cafe_panel_applets_loading == NULL) {
			/* unhide any potential initially hidden toplevel */
			cafe_panel_applet_queue_initial_unhide_toplevels (NULL);
		}

		return FALSE;
	}

	cafe_panel_applets_to_load = g_slist_delete_link (cafe_panel_applets_to_load, l);
	cafe_panel_applets_loading = g_slist_append (cafe_panel_applets_loading, applet);

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (applet->right_stick) {
		if (!panel_widget->packed)
			applet->position = panel_widget->size - applet->position;
		else
			applet->position = -1;
	}

	/* We load applets asynchronously, so we specifically don't call
	 * cafe_panel_applet_stop_loading() for this type. However, in case of
	 * failure during the load, we might call cafe_panel_applet_stop_loading()
	 * synchronously, which will make us lose the content of the applet
	 * variable. So we save the type to be sure we always ignore the
	 * applets. */
	applet_type = applet->type;

	switch (applet_type) {
	case PANEL_OBJECT_APPLET:
		cafe_panel_applet_frame_load_from_gsettings (
					panel_widget,
					applet->locked,
					applet->position,
					applet->id);
		break;
	case PANEL_OBJECT_DRAWER:
		drawer_load_from_gsettings (panel_widget,
					applet->locked,
					applet->position,
					applet->id);
		break;
	case PANEL_OBJECT_MENU:
		panel_menu_button_load_from_gsettings (panel_widget,
						   applet->locked,
						   applet->position,
						   TRUE,
						   applet->id);
		break;
	case PANEL_OBJECT_LAUNCHER:
		launcher_load_from_gsettings (panel_widget,
					  applet->locked,
					  applet->position,
					  applet->id);
		break;
	case PANEL_OBJECT_ACTION:
		panel_action_button_load_from_gsettings (
				panel_widget,
				applet->locked,
				applet->position,
				TRUE,
				applet->id);
		break;
	case PANEL_OBJECT_MENU_BAR:
		panel_menu_bar_load_from_gsettings (
				panel_widget,
				applet->locked,
				applet->position,
				TRUE,
				applet->id);
		break;
	case PANEL_OBJECT_SEPARATOR:
		panel_separator_load_from_gsettings (panel_widget,
						 applet->locked,
						 applet->position,
						 applet->id);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Only the real applets will do a late stop_loading */
	if (applet_type != PANEL_OBJECT_APPLET)
		cafe_panel_applet_stop_loading (applet->id);

	return TRUE;
}

void
cafe_panel_applet_queue_applet_to_load (const char      *id,
				   PanelObjectType  type,
				   const char      *toplevel_id,
				   int              position,
				   gboolean         right_stick,
				   gboolean         locked)
{
	CafePanelAppletToLoad *applet;

	if (!toplevel_id) {
		g_warning ("No toplevel on which to load object '%s'\n", id);
		return;
	}

	applet = g_new0 (CafePanelAppletToLoad, 1);

	applet->id          = g_strdup (id);
	applet->type        = type;
	applet->toplevel_id = g_strdup (toplevel_id);
	applet->position    = position;
	applet->right_stick = right_stick != FALSE;
	applet->locked      = locked != FALSE;

	cafe_panel_applets_to_load = g_slist_prepend (cafe_panel_applets_to_load, applet);
}

static int
cafe_panel_applet_compare (const CafePanelAppletToLoad *a,
		      const CafePanelAppletToLoad *b)
{
	int c;

	if ((c = strcmp (a->toplevel_id, b->toplevel_id)))
		return c;
	else if (a->right_stick != b->right_stick)
		return b->right_stick ? -1 : 1;
	else
		return a->position - b->position;
}

void
cafe_panel_applet_load_queued_applets (gboolean initial_load)
{
	if (!cafe_panel_applets_to_load) {
		cafe_panel_applet_queue_initial_unhide_toplevels (NULL);
		return;
	}

	if (cafe_panel_applets_to_load && cafe_panel_applet_unhide_toplevels_timeout == 0) {
		/* Install a timeout to make sure we don't block the
		 * unhiding because of an applet that doesn't load */
		cafe_panel_applet_unhide_toplevels_timeout =
			g_timeout_add_seconds (UNHIDE_TOPLEVELS_TIMEOUT_SECONDS,
					       cafe_panel_applet_queue_initial_unhide_toplevels,
					       NULL);
	}

	cafe_panel_applets_to_load = g_slist_sort (cafe_panel_applets_to_load,
					      (GCompareFunc) cafe_panel_applet_compare);

	if ( ! cafe_panel_applet_have_load_idle) {
		/* on panel startup, we don't care about redraws of the
		 * toplevels since they are hidden, so we give a higher
		 * priority to loading of applets */
		if (initial_load)
			g_idle_add_full (G_PRIORITY_HIGH_IDLE,
					 cafe_panel_applet_load_idle_handler,
					 NULL, NULL);
		else
			g_idle_add (cafe_panel_applet_load_idle_handler, NULL);

		cafe_panel_applet_have_load_idle = TRUE;
	}
}

static const char* cafe_panel_applet_get_toplevel_id(AppletInfo* applet)
{
	PanelWidget* panel_widget;

	g_return_val_if_fail(applet != NULL, NULL);
	g_return_val_if_fail(CTK_IS_WIDGET(applet->widget), NULL);

	panel_widget = cafe_panel_applet_get_panel_widget(applet);

	if (!panel_widget)
	{
		return NULL;
	}

	return panel_profile_get_toplevel_id(panel_widget->toplevel);
}

static gboolean
cafe_panel_applet_position_save_timeout (gpointer dummy)
{
	GSList *l;

	queued_position_source = 0;

	for (l = queued_position_saves; l; l = l->next) {
		AppletInfo *info = l->data;

		cafe_panel_applet_save_position (info, info->id, TRUE);
	}

	g_slist_free (queued_position_saves);
	queued_position_saves = NULL;

	return FALSE;
}

void
cafe_panel_applet_save_position (AppletInfo *applet_info,
			    const char *id,
			    gboolean    immediate)
{
	PanelWidget       *panel_widget;
	const char        *toplevel_id;
	char              *old_toplevel_id;
	gboolean           right_stick;
	gboolean           locked;
	int                position;

	g_return_if_fail (applet_info != NULL);

	if (!immediate) {
		if (!queued_position_source)
			queued_position_source =
				g_timeout_add_seconds (1,
						       (GSourceFunc) cafe_panel_applet_position_save_timeout,
						       NULL);

		if (!g_slist_find (queued_position_saves, applet_info))
			queued_position_saves =
				g_slist_prepend (queued_position_saves, applet_info);

		return;
	}

	if (!(toplevel_id = cafe_panel_applet_get_toplevel_id (applet_info)))
		return;

	panel_widget = cafe_panel_applet_get_panel_widget (applet_info);

	/* FIXME: Instead of getting keys, comparing and setting, there
	   should be a dirty flag */

	old_toplevel_id = g_settings_get_string (applet_info->settings, PANEL_OBJECT_TOPLEVEL_ID_KEY);
	if (old_toplevel_id == NULL || strcmp (old_toplevel_id, toplevel_id) != 0)
		g_settings_set_string (applet_info->settings, PANEL_OBJECT_TOPLEVEL_ID_KEY, toplevel_id);
	g_free (old_toplevel_id);

	/* Note: changing some properties of the panel that may not be locked down
	   (e.g. background) can change the state of the "panel_right_stick" and
	   "position" properties of an applet that may in fact be locked down.
	   So check if these are writable before attempting to write them */

	locked = panel_widget_get_applet_locked (panel_widget, applet_info->widget) ? 1 : 0;
	if (g_settings_get_boolean (applet_info->settings, PANEL_OBJECT_LOCKED_KEY) ? 1 : 0 != locked)
		g_settings_set_boolean (applet_info->settings, PANEL_OBJECT_LOCKED_KEY, locked);

	if (locked) {
		// Until position calculations are refactored to fix the issue of the panel applets
		// getting reordered on resolution changes...
		// .. don't save position/right-stick on locked applets
		return;
	}

	right_stick = panel_is_applet_right_stick (applet_info->widget) ? 1 : 0;
	if (g_settings_is_writable (applet_info->settings, PANEL_OBJECT_PANEL_RIGHT_STICK_KEY) &&
	    (g_settings_get_boolean (applet_info->settings, PANEL_OBJECT_PANEL_RIGHT_STICK_KEY) ? 1 : 0) != right_stick)
		g_settings_set_boolean (applet_info->settings, PANEL_OBJECT_PANEL_RIGHT_STICK_KEY, right_stick);

	position = cafe_panel_applet_get_position (applet_info);
	if (right_stick && !panel_widget->packed)
		position = panel_widget->size - position;

	if (g_settings_is_writable (applet_info->settings, PANEL_OBJECT_POSITION_KEY) &&
	    g_settings_get_int (applet_info->settings, PANEL_OBJECT_POSITION_KEY) != position)
		g_settings_set_int (applet_info->settings, PANEL_OBJECT_POSITION_KEY, position);
}

const char *
cafe_panel_applet_get_id (AppletInfo *info)
{
	if (!info)
		return NULL;

	return info->id;
}

const char *
cafe_panel_applet_get_id_by_widget (GtkWidget *applet_widget)
{
	GSList *l;

	if (!applet_widget)
		return NULL;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (info->widget == applet_widget)
			return info->id;
	}

	return NULL;
}

AppletInfo *
cafe_panel_applet_get_by_id (const char *id)
{
	GSList *l;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (!strcmp (info->id, id))
			return info;
	}

	return NULL;
}

GSList *
cafe_panel_applet_list_applets (void)
{
	return registered_applets;
}

AppletInfo *
cafe_panel_applet_get_by_type (PanelObjectType object_type, GdkScreen *screen)
{
	GSList *l;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (info->type == object_type) {
			if (screen) {
				if (screen == ctk_widget_get_screen (info->widget))
					return info;
			} else
				return info;
		}
	}

	return NULL;
}

AppletInfo *
cafe_panel_applet_register (GtkWidget       *applet,
		       gpointer         data,
		       GDestroyNotify   data_destroy,
		       PanelWidget     *panel,
		       gboolean         locked,
		       gint             pos,
		       gboolean         exactpos,
		       PanelObjectType  type,
		       const char      *id)
{
	AppletInfo *info;
	gchar *path;
	gchar *locked_changed;

	g_return_val_if_fail (applet != NULL && panel != NULL, NULL);

	if (ctk_widget_get_has_window (applet))
		ctk_widget_set_events (applet, (ctk_widget_get_events (applet) |
						APPLET_EVENT_MASK) &
				       ~( GDK_POINTER_MOTION_MASK |
					  GDK_POINTER_MOTION_HINT_MASK));

	info = g_new0 (AppletInfo, 1);
	info->type         = type;
	info->widget       = applet;
	info->menu         = NULL;
	info->data         = data;
	info->data_destroy = data_destroy;
	info->user_menu    = NULL;
	info->move_item    = NULL;
	info->id           = g_strdup (id);

	path = g_strdup_printf (PANEL_OBJECT_PATH "%s/", id);
	info->settings = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
	g_free (path);

	g_object_set_data (G_OBJECT (applet), "applet_info", info);

	if (type != PANEL_OBJECT_APPLET)
		panel_lockdown_notify_add (G_CALLBACK (cafe_panel_applet_recreate_menu),
					   info);

	locked_changed = g_strdup_printf ("changed::%s", PANEL_OBJECT_LOCKED_KEY);
	g_signal_connect (info->settings,
					  locked_changed,
					  G_CALLBACK (cafe_panel_applet_locked_change_notify),
					  G_OBJECT (applet));
	g_free (locked_changed);

	if (type == PANEL_OBJECT_DRAWER) {
		Drawer *drawer = data;
		PanelWidget *assoc_panel;

		assoc_panel = panel_toplevel_get_panel_widget (drawer->toplevel);

		g_object_set_data (G_OBJECT (applet),
				   CAFE_PANEL_APPLET_ASSOC_PANEL_KEY, assoc_panel);
		assoc_panel->master_widget = applet;
		g_object_add_weak_pointer (
			G_OBJECT (applet), (gpointer *) &assoc_panel->master_widget);
	}

	g_object_set_data (G_OBJECT (applet),
			   CAFE_PANEL_APPLET_FORBIDDEN_PANELS, NULL);

	registered_applets = g_slist_append (registered_applets, info);

	if (panel_widget_add (panel, applet, locked, pos, exactpos) == -1 &&
	    panel_widget_add (panel, applet, locked, 0, TRUE) == -1) {
		GSList *l;

		for (l = panels; l; l = l->next) {
			panel = PANEL_WIDGET (l->data);

			if (panel_widget_add (panel, applet, locked, 0, TRUE) != -1)
				break;
		}

		if (!l) {
			g_warning (_("Cannot find an empty spot"));
			panel_profile_delete_object (info);
			return NULL;
		}
	}

	if (BUTTON_IS_WIDGET (applet) ||
	    ctk_widget_get_has_window (applet)) {
		g_signal_connect (applet, "button_press_event",
				  G_CALLBACK (applet_button_press),
				  info);

		g_signal_connect (applet, "popup_menu",
				  G_CALLBACK (applet_popup_menu),
				  info);
	}

	g_signal_connect (applet, "destroy",
			  G_CALLBACK (cafe_panel_applet_destroy),
			  info);

	cafe_panel_applet_set_dnd_enabled (info, !locked);

	ctk_widget_show_all (applet);

	orientation_change (info, panel);
	size_change (info, panel);
	back_change (info, panel);

	if (type != PANEL_OBJECT_APPLET)
		ctk_widget_grab_focus (applet);
	else
		ctk_widget_child_focus (applet, CTK_DIR_TAB_FORWARD);

	return info;
}

int
cafe_panel_applet_get_position (AppletInfo *applet)
{
	AppletData *applet_data;

	g_return_val_if_fail (applet != NULL, 0);
	g_return_val_if_fail (G_IS_OBJECT (applet->widget), 0);

	applet_data = g_object_get_data (G_OBJECT (applet->widget), CAFE_PANEL_APPLET_DATA);

	return applet_data->pos;
}

gboolean
cafe_panel_applet_can_freely_move (AppletInfo *applet)
{
	if (panel_lockdown_get_locked_down ())
		return FALSE;

	if (!g_settings_is_writable (applet->settings, PANEL_OBJECT_POSITION_KEY))
		return FALSE;

	if (!g_settings_is_writable (applet->settings, PANEL_OBJECT_TOPLEVEL_ID_KEY))
		return FALSE;

	if (!g_settings_is_writable (applet->settings, PANEL_OBJECT_PANEL_RIGHT_STICK_KEY))
		return FALSE;

	return TRUE;
}

gboolean
cafe_panel_applet_lockable (AppletInfo *applet)
{
	if (panel_lockdown_get_locked_down ())
		return FALSE;


	return g_settings_is_writable (applet->settings, PANEL_OBJECT_LOCKED_KEY);
}
