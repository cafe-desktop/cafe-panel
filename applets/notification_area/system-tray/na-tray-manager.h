/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* na-tray-manager.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Used to be: eggtraymanager.h
 */

#ifndef __NA_TRAY_MANAGER_H__
#define __NA_TRAY_MANAGER_H__

#ifdef CDK_WINDOWING_X11
#include <cdk/cdkx.h>
#endif
#include <ctk/ctk.h>

#include "na-tray-child.h"

G_BEGIN_DECLS

#define NA_TYPE_TRAY_MANAGER			(na_tray_manager_get_type ())
#define NA_TRAY_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NA_TYPE_TRAY_MANAGER, NaTrayManager))
#define NA_TRAY_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), NA_TYPE_TRAY_MANAGER, NaTrayManagerClass))
#define NA_IS_TRAY_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NA_TYPE_TRAY_MANAGER))
#define NA_IS_TRAY_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), NA_TYPE_TRAY_MANAGER))
#define NA_TRAY_MANAGER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), NA_TYPE_TRAY_MANAGER, NaTrayManagerClass))

typedef struct _NaTrayManager	    NaTrayManager;
typedef struct _NaTrayManagerClass  NaTrayManagerClass;

struct _NaTrayManager
{
  GObject parent_instance;

#ifdef CDK_WINDOWING_X11
  CdkAtom selection_atom;
  Atom    opcode_atom;
  Atom message_data_atom;
#endif

  CtkWidget *invisible;
  CdkScreen *screen;
  CtkOrientation orientation;
  gint padding;
  gint icon_size;
  CdkRGBA fg;
  CdkRGBA error;
  CdkRGBA warning;
  CdkRGBA success;

  GList *messages;
  GHashTable *socket_table;
};

struct _NaTrayManagerClass
{
  GObjectClass parent_class;

  void (* tray_icon_added)   (NaTrayManager      *manager,
			      NaTrayChild        *child);
  void (* tray_icon_removed) (NaTrayManager      *manager,
			      NaTrayChild        *child);

  void (* message_sent)      (NaTrayManager      *manager,
			      NaTrayChild        *child,
			      const gchar        *message,
			      glong               id,
			      glong               timeout);

  void (* message_cancelled) (NaTrayManager      *manager,
			      NaTrayChild        *child,
			      glong               id);

  void (* lost_selection)    (NaTrayManager      *manager);
};

GType           na_tray_manager_get_type        (void);

gboolean        na_tray_manager_check_running   (CdkScreen          *screen);
NaTrayManager  *na_tray_manager_new             (void);
gboolean        na_tray_manager_manage_screen   (NaTrayManager      *manager,
						 CdkScreen          *screen);
void            na_tray_manager_set_orientation (NaTrayManager      *manager,
						 CtkOrientation      orientation);
CtkOrientation  na_tray_manager_get_orientation (NaTrayManager      *manager);
void            na_tray_manager_set_padding     (NaTrayManager      *manager,
						 gint                padding);
void            na_tray_manager_set_icon_size   (NaTrayManager      *manager,
						 gint                padding);
void            na_tray_manager_set_colors      (NaTrayManager      *manager,
						 CdkRGBA            *fg,
						 CdkRGBA            *error,
						 CdkRGBA            *warning,
						 CdkRGBA            *success);

G_END_DECLS

#endif /* __NA_TRAY_MANAGER_H__ */
