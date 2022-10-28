/*
 * panel-applet-frame-dbus.h: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_FRAME_DBUS_H__
#define __PANEL_APPLET_FRAME_DBUS_H__

#include <panel-applet-frame.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PANEL_TYPE_APPLET_FRAME_DBUS         (cafe_panel_applet_frame_dbus_get_type ())
#define MATE_PANEL_APPLET_FRAME_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME_DBUS, CafePanelAppletFrameDBus))
#define MATE_PANEL_APPLET_FRAME_DBUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME_DBUS, CafePanelAppletFrameDBusClass))
#define PANEL_IS_APPLET_FRAME_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME_DBUS))
#define PANEL_IS_APPLET_FRAME_DBUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME_DBUS))
#define MATE_PANEL_APPLET_FRAME_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME_DBUS, CafePanelAppletFrameDBusClass))

typedef struct _CafePanelAppletFrameDBus        CafePanelAppletFrameDBus;
typedef struct _CafePanelAppletFrameDBusClass   CafePanelAppletFrameDBusClass;
typedef struct _CafePanelAppletFrameDBusPrivate CafePanelAppletFrameDBusPrivate;

struct _CafePanelAppletFrameDBusClass {
        CafePanelAppletFrameClass parent_class;
};

struct _CafePanelAppletFrameDBus{
	CafePanelAppletFrame parent;

	CafePanelAppletFrameDBusPrivate  *priv;
};

GType     cafe_panel_applet_frame_dbus_get_type           (void) G_GNUC_CONST;

gboolean  cafe_panel_applet_frame_dbus_load               (const gchar                 *iid,
						      CafePanelAppletFrameActivating  *frame_act);

#ifdef __cplusplus
}
#endif

#endif /* __PANEL_APPLET_FRAME_DBUS_H__ */
