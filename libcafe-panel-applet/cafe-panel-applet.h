/*
 * cafe-panel-applet.h: panel applet writing API.
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
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
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __CAFE_PANEL_APPLET_H__
#define __CAFE_PANEL_APPLET_H__

#include <glib.h>
#include <ctk/ctk.h>
#include <cdk/cdk.h>
#include <cairo.h>
#include <cairo-gobject.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CAFE_PANEL_APPLET_ORIENT_UP,
	CAFE_PANEL_APPLET_ORIENT_DOWN,
	CAFE_PANEL_APPLET_ORIENT_LEFT,
	CAFE_PANEL_APPLET_ORIENT_RIGHT
#define CAFE_PANEL_APPLET_ORIENT_FIRST CAFE_PANEL_APPLET_ORIENT_UP
#define CAFE_PANEL_APPLET_ORIENT_LAST  CAFE_PANEL_APPLET_ORIENT_RIGHT
} CafePanelAppletOrient;

#define PANEL_TYPE_APPLET              (cafe_panel_applet_get_type ())
#define CAFE_PANEL_APPLET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET, CafePanelApplet))
#define CAFE_PANEL_APPLET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET, CafePanelAppletClass))
#define PANEL_IS_APPLET(o)             (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET))
#define PANEL_IS_APPLET_CLASS(k)       (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET))
#define CAFE_PANEL_APPLET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET, CafePanelAppletClass))

typedef enum {
	PANEL_NO_BACKGROUND,
	PANEL_COLOR_BACKGROUND,
	PANEL_PIXMAP_BACKGROUND
} CafePanelAppletBackgroundType;

typedef enum {
	CAFE_PANEL_APPLET_FLAGS_NONE   = 0,
	CAFE_PANEL_APPLET_EXPAND_MAJOR = 1 << 0,
	CAFE_PANEL_APPLET_EXPAND_MINOR = 1 << 1,
	CAFE_PANEL_APPLET_HAS_HANDLE   = 1 << 2
#define CAFE_PANEL_APPLET_FLAGS_ALL (CAFE_PANEL_APPLET_EXPAND_MAJOR|CAFE_PANEL_APPLET_EXPAND_MINOR|CAFE_PANEL_APPLET_HAS_HANDLE)
} CafePanelAppletFlags;

typedef struct _CafePanelApplet        CafePanelApplet;
typedef struct _CafePanelAppletClass   CafePanelAppletClass;
typedef struct _CafePanelAppletPrivate CafePanelAppletPrivate;

typedef gboolean (*CafePanelAppletFactoryCallback) (CafePanelApplet* applet, const gchar *iid, gpointer user_data);

struct _CafePanelApplet {
	CtkEventBox event_box;

	CafePanelAppletPrivate* priv;
};

struct _CafePanelAppletClass {
	CtkEventBoxClass event_box_class;

	void (*change_orient) (CafePanelApplet* applet, CafePanelAppletOrient orient);

	void (*change_size) (CafePanelApplet* applet, guint size);

	void (*change_background) (CafePanelApplet *applet, CafePanelAppletBackgroundType type, CdkRGBA* color, cairo_pattern_t *pattern);

	void (*move_focus_out_of_applet) (CafePanelApplet* frame, CtkDirectionType direction);
};

GType cafe_panel_applet_get_type(void) G_GNUC_CONST;

CtkWidget* cafe_panel_applet_new(void);

CafePanelAppletOrient cafe_panel_applet_get_orient(CafePanelApplet* applet);
guint cafe_panel_applet_get_size(CafePanelApplet* applet);
CafePanelAppletBackgroundType cafe_panel_applet_get_background (CafePanelApplet *applet, /* return values */ CdkRGBA* color, cairo_pattern_t** pattern);
void cafe_panel_applet_set_background_widget(CafePanelApplet* applet, CtkWidget* widget);

gchar* cafe_panel_applet_get_preferences_path(CafePanelApplet* applet);

CafePanelAppletFlags cafe_panel_applet_get_flags(CafePanelApplet* applet);
void cafe_panel_applet_set_flags(CafePanelApplet* applet, CafePanelAppletFlags flags);

void cafe_panel_applet_set_size_hints(CafePanelApplet* applet, const int* size_hints, int n_elements, int base_size);

gboolean cafe_panel_applet_get_locked_down(CafePanelApplet* applet);

// Does nothing when not on X11
void cafe_panel_applet_request_focus(CafePanelApplet* applet, guint32 timestamp);

void cafe_panel_applet_setup_menu(CafePanelApplet* applet, const gchar* xml, CtkActionGroup* action_group);
void cafe_panel_applet_setup_menu_from_file(CafePanelApplet* applet, const gchar* filename, CtkActionGroup* action_group);
void cafe_panel_applet_setup_menu_from_resource (CafePanelApplet    *applet,
                                                 const gchar        *resource_path,
                                                 CtkActionGroup     *action_group);

int cafe_panel_applet_factory_main(const gchar* factory_id,gboolean  out_process, GType applet_type, CafePanelAppletFactoryCallback callback, gpointer data);

int  cafe_panel_applet_factory_setup_in_process (const gchar               *factory_id,
							  GType                      applet_type,
							  CafePanelAppletFactoryCallback callback,
							  gpointer                   data);


/*
 * These macros are getting a bit unwieldy.
 *
 * Things to define for these:
 *	+ required if Native Language Support is enabled (ENABLE_NLS):
 *                   GETTEXT_PACKAGE and CAFELOCALEDIR
 */

#if !defined(ENABLE_NLS)
	#define _CAFE_PANEL_APPLET_SETUP_GETTEXT(call_textdomain) \
		do { } while (0)
#else /* defined(ENABLE_NLS) */
	#include <libintl.h>
	#define _CAFE_PANEL_APPLET_SETUP_GETTEXT(call_textdomain) \
	do { \
		bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR); \
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8"); \
		if (call_textdomain) \
			textdomain (GETTEXT_PACKAGE); \
	} while (0)
#endif /* !defined(ENABLE_NLS) */

#define CAFE_PANEL_APPLET_OUT_PROCESS_FACTORY(factory_id, type, name, callback, data) \
int main(int argc, char* argv[]) \
{ \
	GOptionContext* context; \
	GError* error; \
	int retval; \
	 \
	_CAFE_PANEL_APPLET_SETUP_GETTEXT (TRUE); \
	 \
	context = g_option_context_new(""); \
	g_option_context_add_group (context, ctk_get_option_group(TRUE)); \
	 \
	error = NULL; \
	if (!g_option_context_parse (context, &argc, &argv, &error)) \
	{ \
		if (error) \
		{ \
			g_printerr ("Cannot parse arguments: %s.\n", error->message); \
			g_error_free (error); \
		} \
		else \
		{ \
			g_printerr ("Cannot parse arguments.\n"); \
		} \
		g_option_context_free (context); \
		return 1; \
	} \
	 \
	ctk_init (&argc, &argv); \
	 \
	retval = cafe_panel_applet_factory_main (factory_id,TRUE, type, callback, data); \
	g_option_context_free (context); \
	 \
	return retval; \
}

#define CAFE_PANEL_APPLET_IN_PROCESS_FACTORY(factory_id, type, descr, callback, data) \
gboolean _cafe_panel_applet_shlib_factory (void);	\
G_MODULE_EXPORT gint _cafe_panel_applet_shlib_factory(void) \
{ \
	_CAFE_PANEL_APPLET_SETUP_GETTEXT(FALSE); \
return cafe_panel_applet_factory_setup_in_process (factory_id, type,                 \
                                               callback, data);  \
}

#ifdef __cplusplus
}
#endif

#endif /* __CAFE_PANEL_APPLET_H__ */
