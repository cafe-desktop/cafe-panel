/* na-tray-manager.c
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
 * Used to be: eggtraymanager.c
 */

#include <config.h>

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include <string.h>
#include <libintl.h>

#include "na-tray-manager.h"

#include <ctk/ctk.h>
#include <glib/gi18n.h>
#include <cdk/cdkx.h>
#include <X11/Xatom.h>

#include "na-marshal.h"

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  MESSAGE_SENT,
  MESSAGE_CANCELLED,
  LOST_SELECTION,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ORIENTATION
};

typedef struct
{
  long id, len;
  long remaining_len;

  long timeout;
  char *str;
#ifdef CDK_WINDOWING_X11
  Window window;
#endif
} PendingMessage;

static guint manager_signals[LAST_SIGNAL] = { 0 };

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

#ifdef CDK_WINDOWING_X11
static gboolean na_tray_manager_check_running_screen_x11 (CdkScreen *screen);
#endif

static void na_tray_manager_finalize     (GObject      *object);
static void na_tray_manager_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void na_tray_manager_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

static void na_tray_manager_unmanage (NaTrayManager *manager);

G_DEFINE_TYPE (NaTrayManager, na_tray_manager, G_TYPE_OBJECT)

static void
na_tray_manager_init (NaTrayManager *manager)
{
  manager->invisible = NULL;
  manager->socket_table = g_hash_table_new (NULL, NULL);

  manager->padding = 0;
  manager->icon_size = 0;

  manager->fg.red = 0.0;
  manager->fg.green = 0.0;
  manager->fg.blue = 0.0;
  manager->fg.alpha = 1.0;

  manager->error.red = 1.0;
  manager->error.green = 0.0;
  manager->error.blue = 0.0;
  manager->error.alpha = 1.0;

  manager->warning.red = 1.0;
  manager->warning.green = 1.0;
  manager->warning.blue = 0.0;
  manager->warning.alpha = 1.0;

  manager->success.red = 0.0;
  manager->success.green = 1.0;
  manager->success.blue = 0.0;
  manager->success.alpha = 1.0;
}

static void
na_tray_manager_class_init (NaTrayManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = na_tray_manager_finalize;
  gobject_class->set_property = na_tray_manager_set_property;
  gobject_class->get_property = na_tray_manager_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      "orientation",
						      "orientation",
						      CTK_TYPE_ORIENTATION,
						      CTK_ORIENTATION_HORIZONTAL,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT |
						      G_PARAM_STATIC_NAME |
						      G_PARAM_STATIC_NICK |
						      G_PARAM_STATIC_BLURB));

  manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray_icon_added",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CTK_TYPE_SOCKET);

  manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray_icon_removed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CTK_TYPE_SOCKET);
  manager_signals[MESSAGE_SENT] =
    g_signal_new ("message_sent",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_sent),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_STRING_LONG_LONG,
		  G_TYPE_NONE, 4,
		  CTK_TYPE_SOCKET,
		  G_TYPE_STRING,
		  G_TYPE_LONG,
		  G_TYPE_LONG);
  manager_signals[MESSAGE_CANCELLED] =
    g_signal_new ("message_cancelled",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_cancelled),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_LONG,
		  G_TYPE_NONE, 2,
		  CTK_TYPE_SOCKET,
		  G_TYPE_LONG);
  manager_signals[LOST_SELECTION] =
    g_signal_new ("lost_selection",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, lost_selection),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
na_tray_manager_finalize (GObject *object)
{
  NaTrayManager *manager;

  manager = NA_TRAY_MANAGER (object);

  na_tray_manager_unmanage (manager);

  g_list_free (manager->messages);
  g_hash_table_destroy (manager->socket_table);

  G_OBJECT_CLASS (na_tray_manager_parent_class)->finalize (object);
}

static void
na_tray_manager_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      na_tray_manager_set_orientation (manager, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_tray_manager_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, manager->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

NaTrayManager *
na_tray_manager_new (void)
{
  NaTrayManager *manager;

  manager = g_object_new (NA_TYPE_TRAY_MANAGER, NULL);

  return manager;
}

#ifdef CDK_WINDOWING_X11

static gboolean
na_tray_manager_plug_removed (CtkSocket       *socket,
			      NaTrayManager   *manager)
{
  NaTrayChild *child = NA_TRAY_CHILD (socket);

  g_hash_table_remove (manager->socket_table,
                       GINT_TO_POINTER (child->icon_window));
  g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, child);

  /* This destroys the socket. */
  return FALSE;
}

static void
na_tray_manager_handle_dock_request (NaTrayManager       *manager,
				     XClientMessageEvent *xevent)
{
  Window icon_window = xevent->data.l[2];
  CtkWidget *child;

  if (g_hash_table_lookup (manager->socket_table,
                           GINT_TO_POINTER (icon_window)))
    {
      /* We already got this notification earlier, ignore this one */
      return;
    }

  child = na_tray_child_new (manager->screen, icon_window);
  if (child == NULL) /* already gone or other error */
    return;

  g_signal_emit (manager, manager_signals[TRAY_ICON_ADDED], 0,
		 child);

  /* If the child wasn't attached, then destroy it */

  if (!CTK_IS_WINDOW (ctk_widget_get_toplevel (CTK_WIDGET (child))))
    {
      ctk_widget_destroy (child);
      return;
    }

  g_signal_connect (child, "plug_removed",
		    G_CALLBACK (na_tray_manager_plug_removed), manager);

  ctk_socket_add_id (CTK_SOCKET (child), icon_window);

  if (!ctk_socket_get_plug_window (CTK_SOCKET (child)))
    {
      /* Embedding failed, we won't get a plug-removed signal */
      /* This signal destroys the socket */
      g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, child);
      return;
    }

  g_hash_table_insert (manager->socket_table,
                       GINT_TO_POINTER (icon_window), child);
  ctk_widget_show (child);
}

static void
pending_message_free (PendingMessage *message)
{
  g_free (message->str);
  g_free (message);
}

static void
na_tray_manager_handle_message_data (NaTrayManager *manager,
                                     XClientMessageEvent *xevent)
{
  GList               *p;
  int                  len;

  /* Try to see if we can find the pending message in the list */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window)
	{
	  /* Append the message */
	  len = MIN (msg->remaining_len, 20);

	  memcpy ((msg->str + msg->len - msg->remaining_len),
		  &xevent->data, len);
	  msg->remaining_len -= len;

	  if (msg->remaining_len == 0)
	    {
	      CtkSocket *socket;

	      socket = g_hash_table_lookup (manager->socket_table,
                                            GINT_TO_POINTER (msg->window));

	      if (socket)
		  g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
				 socket, msg->str, msg->id, msg->timeout);

	      pending_message_free (msg);
	      manager->messages = g_list_remove_link (manager->messages, p);
              g_list_free_1 (p);
	    }

          break;
	}
    }
}

static void
na_tray_manager_handle_begin_message (NaTrayManager       *manager,
				      XClientMessageEvent *xevent)
{
  CtkSocket      *socket;
  GList          *p;
  PendingMessage *msg;
  long            timeout;
  long            len;
  long            id;

  socket = g_hash_table_lookup (manager->socket_table,
                                GINT_TO_POINTER (xevent->window));
  /* we don't know about this tray icon, so ignore the message */
  if (!socket)
    return;

  timeout = xevent->data.l[2];
  len     = xevent->data.l[3];
  id      = xevent->data.l[4];

  /* Check if the same message is already in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *pmsg = p->data;

      if (xevent->window == pmsg->window &&
	  id == pmsg->id)
	{
	  /* Hmm, we found it, now remove it */
	  pending_message_free (pmsg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  if (len == 0)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
                     socket, "", id, timeout);
    }
  else
    {
      /* Now add the new message to the queue */
      msg = g_new0 (PendingMessage, 1);
      msg->window = xevent->window;
      msg->timeout = timeout;
      msg->len = len;
      msg->id = id;
      msg->remaining_len = msg->len;
      msg->str = g_malloc (msg->len + 1);
      msg->str[msg->len] = '\0';
      manager->messages = g_list_prepend (manager->messages, msg);
    }
}

static void
na_tray_manager_handle_cancel_message (NaTrayManager       *manager,
				       XClientMessageEvent *xevent)
{
  GList     *p;
  CtkSocket *socket;
  long       id;

  id = xevent->data.l[2];

  /* Check if the message is in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window &&
	  id == msg->id)
	{
	  pending_message_free (msg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  socket = g_hash_table_lookup (manager->socket_table,
                                GINT_TO_POINTER (xevent->window));

  if (socket)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_CANCELLED], 0,
		     socket, xevent->data.l[2]);
    }
}

static CdkFilterReturn
na_tray_manager_window_filter (CdkXEvent *xev,
                               CdkEvent  *event,
                               gpointer   data)
{
  XEvent        *xevent = (CdkXEvent *)xev;
  NaTrayManager *manager = data;

  if (xevent->type == ClientMessage)
    {
      /* We handle this client message here. See comment in
       * na_tray_manager_handle_client_message_opcode() for details */
      if (xevent->xclient.message_type == manager->opcode_atom &&
          xevent->xclient.data.l[1]    == SYSTEM_TRAY_REQUEST_DOCK)
	{
          na_tray_manager_handle_dock_request (manager,
                                               (XClientMessageEvent *) xevent);
          return CDK_FILTER_REMOVE;
	}
      /* _NET_SYSTEM_TRAY_OPCODE: SYSTEM_TRAY_BEGIN_MESSAGE */
      else if (xevent->xclient.message_type == manager->opcode_atom &&
               xevent->xclient.data.l[1] == SYSTEM_TRAY_BEGIN_MESSAGE)
        {
          na_tray_manager_handle_begin_message (manager,
                                                (XClientMessageEvent *) event);
          return CDK_FILTER_REMOVE;
        }
      /* _NET_SYSTEM_TRAY_OPCODE: SYSTEM_TRAY_CANCEL_MESSAGE */
      else if (xevent->xclient.message_type == manager->opcode_atom &&
               xevent->xclient.data.l[1] == SYSTEM_TRAY_CANCEL_MESSAGE)
        {
          na_tray_manager_handle_cancel_message (manager,
                                                 (XClientMessageEvent *) event);
          return CDK_FILTER_REMOVE;
        }
      /* _NET_SYSTEM_TRAY_MESSAGE_DATA */
      else if (xevent->xclient.message_type == manager->message_data_atom)
        {
          na_tray_manager_handle_message_data (manager,
                                               (XClientMessageEvent *) event);
          return CDK_FILTER_REMOVE;
        }
    }
  else if (xevent->type == SelectionClear)
    {
      g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
      na_tray_manager_unmanage (manager);
    }

  return CDK_FILTER_CONTINUE;
}

#if 0
//FIXME investigate why this doesn't work
static gboolean
na_tray_manager_selection_clear_event (CtkWidget         *widget,
                                       CdkEventSelection *event,
                                       NaTrayManager     *manager)
{
  g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
  na_tray_manager_unmanage (manager);

  return FALSE;
}
#endif
#endif

static void
na_tray_manager_unmanage (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkDisplay *display;
  guint32     timestamp;
  CtkWidget  *invisible;
  CdkWindow  *window;

  if (manager->invisible == NULL)
    return;

  invisible = manager->invisible;
  window = ctk_widget_get_window (invisible);

  g_assert (CTK_IS_INVISIBLE (invisible));
  g_assert (ctk_widget_get_realized (invisible));
  g_assert (CDK_IS_WINDOW (window));

  display = ctk_widget_get_display (invisible);

  if (cdk_selection_owner_get_for_display (display, manager->selection_atom) ==
      window)
    {
      timestamp = cdk_x11_get_server_time (window);
      cdk_selection_owner_set_for_display (display,
                                           NULL,
                                           manager->selection_atom,
                                           timestamp,
                                           TRUE);
    }

  cdk_window_remove_filter (window,
                            na_tray_manager_window_filter, manager);

  manager->invisible = NULL; /* prior to destroy for reentrancy paranoia */
  ctk_widget_destroy (invisible);
  g_object_unref (G_OBJECT (invisible));
#endif
}

static void
na_tray_manager_set_orientation_property (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkWindow  *window;
  CdkDisplay *display;
  Atom        orientation_atom;
  gulong      data[1];

  g_return_if_fail (manager->invisible != NULL);
  window = ctk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  display = ctk_widget_get_display (manager->invisible);
  orientation_atom = cdk_x11_get_xatom_by_name_for_display (display,
                                                            "_NET_SYSTEM_TRAY_ORIENTATION");

  data[0] = manager->orientation == CTK_ORIENTATION_HORIZONTAL ?
		SYSTEM_TRAY_ORIENTATION_HORZ :
		SYSTEM_TRAY_ORIENTATION_VERT;

  XChangeProperty (CDK_DISPLAY_XDISPLAY (display),
		   CDK_WINDOW_XID (window),
                   orientation_atom,
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_visual_property (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkWindow  *window;
  CdkDisplay *display;
  Visual     *xvisual;
  Atom        visual_atom;
  gulong      data[1];

  g_return_if_fail (manager->invisible != NULL);
  window = ctk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  /* The visual property is a hint to the tray icons as to what visual they
   * should use for their windows. If the X server has RGBA colormaps, then
   * we tell the tray icons to use a RGBA colormap and we'll composite the
   * icon onto its parents with real transparency. Otherwise, we just tell
   * the icon to use our colormap, and we'll do some hacks with parent
   * relative backgrounds to simulate transparency.
   */

  display = ctk_widget_get_display (manager->invisible);
  visual_atom = cdk_x11_get_xatom_by_name_for_display (display,
						       "_NET_SYSTEM_TRAY_VISUAL");

  if (cdk_screen_get_rgba_visual (manager->screen) != NULL &&
      cdk_display_supports_composite (display))
    {
      xvisual = CDK_VISUAL_XVISUAL (cdk_screen_get_rgba_visual (manager->screen));
    }
  else
    {
      /* We actually want the visual of the tray where the icons will
       * be embedded. In almost all cases, this will be the same as the visual
       * of the screen.
       */
      xvisual = CDK_VISUAL_XVISUAL (cdk_screen_get_system_visual (manager->screen));
    }

  data[0] = XVisualIDFromVisual (xvisual);

  XChangeProperty (CDK_DISPLAY_XDISPLAY (display),
                   CDK_WINDOW_XID (window),
                   visual_atom,
                   XA_VISUALID, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_padding_property (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkWindow  *window;
  CdkDisplay *display;
  Atom        atom;
  gulong      data[1];

  g_return_if_fail (manager->invisible != NULL);
  window = ctk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  display = ctk_widget_get_display (manager->invisible);
  atom = cdk_x11_get_xatom_by_name_for_display (display,
                                                "_NET_SYSTEM_TRAY_PADDING");

  data[0] = manager->padding;

  XChangeProperty (CDK_DISPLAY_XDISPLAY (display),
                   CDK_WINDOW_XID (window),
                   atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_icon_size_property (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkWindow  *window;
  CdkDisplay *display;
  Atom        atom;
  gulong      data[1];

  g_return_if_fail (manager->invisible != NULL);
  window = ctk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  display = ctk_widget_get_display (manager->invisible);
  atom = cdk_x11_get_xatom_by_name_for_display (display,
                                                "_NET_SYSTEM_TRAY_ICON_SIZE");

  data[0] = manager->icon_size;

  XChangeProperty (CDK_DISPLAY_XDISPLAY (display),
                   CDK_WINDOW_XID (window),
                   atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_colors_property (NaTrayManager *manager)
{
#ifdef CDK_WINDOWING_X11
  CdkWindow  *window;
  CdkDisplay *display;
  Atom        atom;
  gulong      data[12];

  g_return_if_fail (manager->invisible != NULL);
  window = ctk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  display = ctk_widget_get_display (manager->invisible);
  atom = cdk_x11_get_xatom_by_name_for_display (display,
                                                "_NET_SYSTEM_TRAY_COLORS");

  data[0] = manager->fg.red * 65535;
  data[1] = manager->fg.green * 65535;
  data[2] = manager->fg.blue * 65535;
  data[3] = manager->error.red * 65535;
  data[4] = manager->error.green * 65535;
  data[5] = manager->error.blue * 65535;
  data[6] = manager->warning.red * 65535;
  data[7] = manager->warning.green * 65535;
  data[8] = manager->warning.blue * 65535;
  data[9] = manager->success.red * 65535;
  data[10] = manager->success.green * 65535;
  data[11] = manager->success.blue * 65535;

  XChangeProperty (CDK_DISPLAY_XDISPLAY (display),
                   CDK_WINDOW_XID (window),
                   atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 12);
#endif
}

#ifdef CDK_WINDOWING_X11

static gboolean
na_tray_manager_manage_screen_x11 (NaTrayManager *manager,
				   CdkScreen     *screen)
{
  CdkDisplay *display;
  Screen     *xscreen;
  CtkWidget  *invisible;
  CdkWindow  *window;
  char       *selection_atom_name;
  guint32     timestamp;

  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

  /* If there's already a manager running on the screen
   * we can't create another one.
   */
#if 0
  if (na_tray_manager_check_running_screen_x11 (screen))
    return FALSE;
#endif

  manager->screen = screen;

  display = cdk_screen_get_display (screen);
  xscreen = CDK_SCREEN_XSCREEN (screen);

  invisible = ctk_invisible_new_for_screen (screen);
  ctk_widget_realize (invisible);

  ctk_widget_add_events (invisible,
                         CDK_PROPERTY_CHANGE_MASK | CDK_STRUCTURE_MASK);

  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					 cdk_x11_screen_get_screen_number (screen));
  manager->selection_atom = cdk_atom_intern (selection_atom_name, FALSE);
  g_free (selection_atom_name);

  manager->invisible = invisible;
  g_object_ref (G_OBJECT (manager->invisible));

  na_tray_manager_set_orientation_property (manager);
  na_tray_manager_set_visual_property (manager);
  na_tray_manager_set_padding_property (manager);
  na_tray_manager_set_icon_size_property (manager);
  na_tray_manager_set_colors_property (manager);

  window = ctk_widget_get_window (invisible);

  timestamp = cdk_x11_get_server_time (window);

  /* Check if we could set the selection owner successfully */
  if (cdk_selection_owner_set_for_display (display,
                                           window,
                                           manager->selection_atom,
                                           timestamp,
                                           TRUE))
    {
      XClientMessageEvent xev;
      CdkAtom             opcode_atom;
      CdkAtom             message_data_atom;

      xev.type = ClientMessage;
      xev.window = RootWindowOfScreen (xscreen);
      xev.message_type = cdk_x11_get_xatom_by_name_for_display (display,
                                                                "MANAGER");

      xev.format = 32;
      xev.data.l[0] = timestamp;
      xev.data.l[1] = cdk_x11_atom_to_xatom_for_display (display,
                                                         manager->selection_atom);
      xev.data.l[2] = CDK_WINDOW_XID (window);
      xev.data.l[3] = 0;	/* manager specific data */
      xev.data.l[4] = 0;	/* manager specific data */

      XSendEvent (CDK_DISPLAY_XDISPLAY (display),
		  RootWindowOfScreen (xscreen),
		  False, StructureNotifyMask, (XEvent *)&xev);

      opcode_atom = cdk_atom_intern ("_NET_SYSTEM_TRAY_OPCODE", FALSE);
      manager->opcode_atom = cdk_x11_atom_to_xatom_for_display (display,
                                                                opcode_atom);

      message_data_atom = cdk_atom_intern ("_NET_SYSTEM_TRAY_MESSAGE_DATA",
                                           FALSE);

      manager->message_data_atom = cdk_x11_atom_to_xatom_for_display (display,
                                                                      message_data_atom);

      /* Add a window filter */
#if 0
      /* This is for when we lose the selection of _NET_SYSTEM_TRAY_Sx */
      g_signal_connect (invisible, "selection-clear-event",
                        G_CALLBACK (na_tray_manager_selection_clear_event),
                        manager);
#endif
      /* This is for SYSTEM_TRAY_REQUEST_DOCK and SelectionClear */
      cdk_window_add_filter (window,
                             na_tray_manager_window_filter, manager);
      return TRUE;
    }
  else
    {
      ctk_widget_destroy (invisible);
      g_object_unref (invisible);
      manager->invisible = NULL;

      manager->screen = NULL;

      return FALSE;
    }
}

#endif

gboolean
na_tray_manager_manage_screen (NaTrayManager *manager,
			       CdkScreen     *screen)
{
  g_return_val_if_fail (CDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

#ifdef CDK_WINDOWING_X11
  return na_tray_manager_manage_screen_x11 (manager, screen);
#else
  return FALSE;
#endif
}

#ifdef CDK_WINDOWING_X11

static gboolean
na_tray_manager_check_running_screen_x11 (CdkScreen *screen)
{
  CdkDisplay *display;
  Atom        selection_atom;
  char       *selection_atom_name;

  display = cdk_screen_get_display (screen);
  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
                                         cdk_x11_screen_get_screen_number (screen));
  selection_atom = cdk_x11_get_xatom_by_name_for_display (display,
                                                          selection_atom_name);
  g_free (selection_atom_name);

  if (XGetSelectionOwner (CDK_DISPLAY_XDISPLAY (display),
                          selection_atom) != None)
    return TRUE;
  else
    return FALSE;
}

#endif

gboolean
na_tray_manager_check_running (CdkScreen *screen)
{
  g_return_val_if_fail (CDK_IS_SCREEN (screen), FALSE);

#ifdef CDK_WINDOWING_X11
  return na_tray_manager_check_running_screen_x11 (screen);
#else
  return FALSE;
#endif
}

void
na_tray_manager_set_orientation (NaTrayManager  *manager,
				 CtkOrientation  orientation)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->orientation != orientation)
    {
      manager->orientation = orientation;

      na_tray_manager_set_orientation_property (manager);

      g_object_notify (G_OBJECT (manager), "orientation");
    }
}

void
na_tray_manager_set_padding (NaTrayManager *manager,
                             gint           padding)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->padding != padding)
    {
      manager->padding = padding;

      na_tray_manager_set_padding_property (manager);
    }
}

void
na_tray_manager_set_icon_size (NaTrayManager *manager,
                               gint           icon_size)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->icon_size != icon_size)
    {
      manager->icon_size = icon_size;

      na_tray_manager_set_icon_size_property (manager);
    }
}

void
na_tray_manager_set_colors (NaTrayManager *manager,
                            CdkRGBA       *fg,
                            CdkRGBA       *error,
                            CdkRGBA       *warning,
                            CdkRGBA       *success)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (!cdk_rgba_equal (&manager->fg, fg) ||
      !cdk_rgba_equal (&manager->error, error) ||
      !cdk_rgba_equal (&manager->warning, warning) ||
      !cdk_rgba_equal (&manager->success, success))
    {
      manager->fg = *fg;
      manager->error = *error;
      manager->warning = *warning;
      manager->success = *success;

      na_tray_manager_set_colors_property (manager);
    }
}

CtkOrientation
na_tray_manager_get_orientation (NaTrayManager *manager)
{
  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), CTK_ORIENTATION_HORIZONTAL);

  return manager->orientation;
}
