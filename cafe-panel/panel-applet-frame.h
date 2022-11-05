/*
 * panel-applet-frame.h: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_FRAME_H__
#define __PANEL_APPLET_FRAME_H__

#include <ctk/ctk.h>

#include "panel-widget.h"
#include "applet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_TYPE_APPLET_FRAME         (cafe_panel_applet_frame_get_type ())
#define CAFE_PANEL_APPLET_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME, CafePanelAppletFrame))
#define CAFE_PANEL_APPLET_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME, CafePanelAppletFrameClass))
#define PANEL_IS_APPLET_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME))
#define PANEL_IS_APPLET_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME))
#define CAFE_PANEL_APPLET_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME, CafePanelAppletFrameClass))

typedef struct _CafePanelAppletFrame        CafePanelAppletFrame;
typedef struct _CafePanelAppletFrameClass   CafePanelAppletFrameClass;
typedef struct _CafePanelAppletFramePrivate CafePanelAppletFramePrivate;

struct _CafePanelAppletFrameClass {
        CtkEventBoxClass parent_class;

	void     (*init_properties)       (CafePanelAppletFrame    *frame);

	void     (*sync_menu_state)       (CafePanelAppletFrame    *frame,
					   gboolean             movable,
					   gboolean             removable,
					   gboolean             lockable,
					   gboolean             locked,
					   gboolean             locked_down);

	void     (*popup_menu)            (CafePanelAppletFrame    *frame,
					   guint                button,
					   guint32              timestamp);

	void     (*change_orientation)    (CafePanelAppletFrame    *frame,
					   PanelOrientation     orientation);

	void     (*change_size)           (CafePanelAppletFrame    *frame,
					   guint                size);

	void     (*change_background)     (CafePanelAppletFrame    *frame,
					   PanelBackgroundType  type);
};

struct _CafePanelAppletFrame {
	CtkEventBox parent;

        CafePanelAppletFramePrivate  *priv;
};

GType cafe_panel_applet_frame_get_type           (void) G_GNUC_CONST;

void  cafe_panel_applet_frame_create             (PanelToplevel       *toplevel,
					     int                  position,
					     const char          *iid);

void  cafe_panel_applet_frame_load_from_gsettings    (PanelWidget         *panel_widget,
					     gboolean             locked,
					     int                  position,
					     const char          *id);

void  cafe_panel_applet_frame_sync_menu_state    (CafePanelAppletFrame    *frame);

void  cafe_panel_applet_frame_change_orientation (CafePanelAppletFrame    *frame,
					     PanelOrientation     orientation);

void  cafe_panel_applet_frame_change_size        (CafePanelAppletFrame    *frame,
					     guint                size);

void  cafe_panel_applet_frame_change_background  (CafePanelAppletFrame    *frame,
					     PanelBackgroundType  type);

void  cafe_panel_applet_frame_set_panel          (CafePanelAppletFrame    *frame,
					     PanelWidget         *panel);


/* For module implementations only */

typedef struct _CafePanelAppletFrameActivating        CafePanelAppletFrameActivating;

CdkScreen        *panel_applet_frame_activating_get_screen      (CafePanelAppletFrameActivating *frame_act);
PanelOrientation  cafe_panel_applet_frame_activating_get_orientation (CafePanelAppletFrameActivating *frame_act);
guint32           cafe_panel_applet_frame_activating_get_size        (CafePanelAppletFrameActivating *frame_act);
gboolean          cafe_panel_applet_frame_activating_get_locked      (CafePanelAppletFrameActivating *frame_act);
gboolean          cafe_panel_applet_frame_activating_get_locked_down (CafePanelAppletFrameActivating *frame_act);
gchar            *cafe_panel_applet_frame_activating_get_conf_path   (CafePanelAppletFrameActivating *frame_act);

void  _cafe_panel_applet_frame_set_iid               (CafePanelAppletFrame           *frame,
						 const gchar                *iid);

void  _cafe_panel_applet_frame_activated             (CafePanelAppletFrame           *frame,
						 CafePanelAppletFrameActivating *frame_act,
						 GError                     *error);

void  _cafe_panel_applet_frame_update_flags          (CafePanelAppletFrame *frame,
						 gboolean          major,
						 gboolean          minor,
						 gboolean          has_handle);

void  _cafe_panel_applet_frame_update_size_hints     (CafePanelAppletFrame *frame,
						 gint             *size_hints,
						 guint             n_elements);

char *_cafe_panel_applet_frame_get_background_string (CafePanelAppletFrame    *frame,
						 PanelWidget         *panel,
						 PanelBackgroundType  type);

void  _cafe_panel_applet_frame_applet_broken         (CafePanelAppletFrame *frame);

void  _cafe_panel_applet_frame_applet_remove         (CafePanelAppletFrame *frame);
void  _cafe_panel_applet_frame_applet_move           (CafePanelAppletFrame *frame);
void  _cafe_panel_applet_frame_applet_lock           (CafePanelAppletFrame *frame,
						 gboolean          locked);
#ifdef __cplusplus
}
#endif

#endif /* __PANEL_APPLET_FRAME_H__ */

