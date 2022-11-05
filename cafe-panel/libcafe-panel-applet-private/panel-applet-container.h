/*
 * panel-applet-container.h: a container for applets.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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
 */

#ifndef __PANEL_APPLET_CONTAINER_H__
#define __PANEL_APPLET_CONTAINER_H__

#include <glib-object.h>
#include <ctk/ctk.h>
#include "panel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_TYPE_APPLET_CONTAINER            (cafe_panel_applet_container_get_type ())
#define CAFE_PANEL_APPLET_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLET_CONTAINER, CafePanelAppletContainer))
#define CAFE_PANEL_APPLET_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PANEL_TYPE_APPLET_CONTAINER, CafePanelAppletContainerClass))
#define PANEL_IS_APPLET_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLET_CONTAINER))
#define PANEL_IS_APPLET_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLET_CONTAINER))
#define CAFE_PANEL_APPLET_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_APPLET_CONTAINER, CafePanelAppletContainerClass))

#define CAFE_PANEL_APPLET_CONTAINER_ERROR           (cafe_panel_applet_container_error_quark())

typedef enum {
	CAFE_PANEL_APPLET_CONTAINER_INVALID_APPLET,
	CAFE_PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY
} CafePanelAppletContainerError;

typedef struct _CafePanelAppletContainer        CafePanelAppletContainer;
typedef struct _CafePanelAppletContainerClass   CafePanelAppletContainerClass;
typedef struct _CafePanelAppletContainerPrivate CafePanelAppletContainerPrivate;

struct _CafePanelAppletContainer {
	CtkEventBox parent;

	CafePanelAppletContainerPrivate *priv;
};

struct _CafePanelAppletContainerClass {
	CtkEventBoxClass parent_class;

	/* Signals */
	void (*applet_broken)          (CafePanelAppletContainer *container);
	void (*applet_move)            (CafePanelAppletContainer *container);
	void (*applet_remove)          (CafePanelAppletContainer *container);
	void (*applet_lock)            (CafePanelAppletContainer *container,
					gboolean              locked);
	void (*child_property_changed) (CafePanelAppletContainer *container,
					const gchar          *property_name,
					GVariant             *value);
};

GType      cafe_panel_applet_container_get_type                (void) G_GNUC_CONST;
GQuark     cafe_panel_applet_container_error_quark             (void) G_GNUC_CONST;
CtkWidget *cafe_panel_applet_container_new                     (void);


void       cafe_panel_applet_container_add                     (CafePanelAppletContainer *container,
							   CdkScreen            *screen,
							   const gchar          *iid,
							   GCancellable        *cancellable,
							   GAsyncReadyCallback  callback,
							   gpointer             user_data,
							   GVariant            *properties);
gboolean   cafe_panel_applet_container_add_finish              (CafePanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);
void       cafe_panel_applet_container_child_popup_menu        (CafePanelAppletContainer *container,
							   guint                 button,
							   guint32               timestamp,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
gboolean   cafe_panel_applet_container_child_popup_menu_finish (CafePanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);

gconstpointer  cafe_panel_applet_container_child_set           (CafePanelAppletContainer *container,
							   const gchar          *property_name,
							   const GVariant       *value,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
gboolean   cafe_panel_applet_container_child_set_finish        (CafePanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);
gconstpointer  cafe_panel_applet_container_child_get           (CafePanelAppletContainer *container,
							   const gchar          *property_name,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
GVariant  *cafe_panel_applet_container_child_get_finish        (CafePanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);

void       cafe_panel_applet_container_cancel_operation (CafePanelAppletContainer *container,
                                                         gconstpointer             operation);

#ifdef __cplusplus
}
#endif

#endif /* __PANEL_APPLET_CONTAINER_H__ */
