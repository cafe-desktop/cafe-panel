/* Marco fixed tooltip routine */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003-2006 Vincent Untz
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

#ifndef HAVE_X11
#error file should only be compiled when HAVE_X11 is enabled
#endif

#include <cdk/cdkx.h>
#include "fixedtip.h"

/* Signals */
enum
{
  CLICKED,
  LAST_SIGNAL
};

static guint fixedtip_signals[LAST_SIGNAL] = { 0 };

struct _NaFixedTipPrivate
{
  CtkWidget      *parent;
  CtkWidget      *label;
  CtkOrientation  orientation;
};

G_DEFINE_TYPE_WITH_PRIVATE (NaFixedTip, na_fixed_tip, CTK_TYPE_WINDOW)

static gboolean
button_press_handler (CtkWidget      *fixedtip,
                      CdkEventButton *event,
                      gpointer        data)
{
  if (event->button == 1 && event->type == CDK_BUTTON_PRESS)
    g_signal_emit (fixedtip, fixedtip_signals[CLICKED], 0);

  return FALSE;
}

static gboolean
na_fixed_tip_draw (CtkWidget *widget, cairo_t *cr)
{
  CtkStyleContext *context;
  CtkStateFlags state;
  int width, height;

  width = ctk_widget_get_allocated_width (widget);
  height = ctk_widget_get_allocated_height (widget);

  state = ctk_widget_get_state_flags (widget);
  context = ctk_widget_get_style_context (widget);
  ctk_style_context_save (context);
  ctk_style_context_add_class (context, CTK_STYLE_CLASS_TOOLTIP);
  ctk_style_context_set_state (context, state);

  cairo_save (cr);
  ctk_render_background (context, cr,
                         0., 0.,
                         (gdouble)width,
                         (gdouble)height);
  cairo_restore (cr);

  ctk_style_context_restore (context);

  return FALSE;
}

static void
na_fixed_tip_class_init (NaFixedTipClass *class)
{
  CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (class);
  widget_class->draw = na_fixed_tip_draw;

  fixedtip_signals[CLICKED] =
    g_signal_new ("clicked",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaFixedTipClass, clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

/* Did you already see this code? Yes, it's ctk_tooltips_ force_window() ;-) */
static void
na_fixed_tip_init (NaFixedTip *fixedtip)
{
  CtkWidget *label;

  fixedtip->priv = na_fixed_tip_get_instance_private (fixedtip);

  ctk_window_set_type_hint (CTK_WINDOW (fixedtip),
                            CDK_WINDOW_TYPE_HINT_TOOLTIP);

  ctk_widget_set_app_paintable (CTK_WIDGET (fixedtip), TRUE);
  ctk_window_set_resizable (CTK_WINDOW (fixedtip), FALSE);
  ctk_widget_set_name (CTK_WIDGET (fixedtip), "ctk-tooltips");
  ctk_container_set_border_width (CTK_CONTAINER (fixedtip), 4);

  label = ctk_label_new (NULL);
  ctk_label_set_line_wrap (CTK_LABEL (label), TRUE);
  ctk_label_set_xalign (CTK_LABEL (label), 0.5);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_widget_show (label);
  ctk_container_add (CTK_CONTAINER (fixedtip), label);
  fixedtip->priv->label = label;

  ctk_widget_add_events (CTK_WIDGET (fixedtip), CDK_BUTTON_PRESS_MASK);

  g_signal_connect (fixedtip, "button_press_event",
                    G_CALLBACK (button_press_handler), NULL);

  fixedtip->priv->orientation = CTK_ORIENTATION_HORIZONTAL;
}

static void
na_fixed_tip_position (NaFixedTip *fixedtip)
{
  CdkScreen      *screen;
  CdkWindow      *parent_window;
  CtkRequisition  req;
  int             root_x;
  int             root_y;
  int             parent_width;
  int             parent_height;
  int             screen_width;
  int             screen_height;

  screen = ctk_widget_get_screen (fixedtip->priv->parent);
  parent_window = ctk_widget_get_window (fixedtip->priv->parent);

  ctk_window_set_screen (CTK_WINDOW (fixedtip), screen);

  ctk_widget_get_preferred_size (CTK_WIDGET (fixedtip), &req, NULL);

  cdk_window_get_origin (parent_window, &root_x, &root_y);
  parent_width = cdk_window_get_width(parent_window);
  parent_height = cdk_window_get_height(parent_window);

  screen_width = WidthOfScreen (cdk_x11_screen_get_xscreen (screen));
  screen_height = HeightOfScreen (cdk_x11_screen_get_xscreen (screen));

  /* pad between panel and message window */
#define PAD 5

  if (fixedtip->priv->orientation == CTK_ORIENTATION_VERTICAL)
    {
      if (root_x <= screen_width / 2)
        root_x += parent_width + PAD;
      else
        root_x -= req.width + PAD;
    }
  else
    {
      if (root_y <= screen_height / 2)
        root_y += parent_height + PAD;
      else
        root_y -= req.height + PAD;
    }

  /* Push onscreen */
  if ((root_x + req.width) > screen_width)
    root_x = screen_width - req.width;

  if ((root_y + req.height) > screen_height)
    root_y = screen_height - req.height;

  ctk_window_move (CTK_WINDOW (fixedtip), root_x, root_y);
}

static void
na_fixed_tip_parent_size_allocated (CtkWidget     *parent,
                                    CtkAllocation *allocation,
                                    NaFixedTip    *fixedtip)
{
  na_fixed_tip_position (fixedtip);
}

static void
na_fixed_tip_parent_screen_changed (CtkWidget  *parent,
                                    CdkScreen  *new_screen,
                                    NaFixedTip *fixedtip)
{
  na_fixed_tip_position (fixedtip);
}

CtkWidget *
na_fixed_tip_new (CtkWidget      *parent,
                  CtkOrientation  orientation)
{
  NaFixedTip *fixedtip;

  g_return_val_if_fail (parent != NULL, NULL);

  fixedtip = g_object_new (NA_TYPE_FIXED_TIP,
                           "type", CTK_WINDOW_POPUP,
                           NULL);

  fixedtip->priv->parent = parent;

#if 0
  //FIXME: would be nice to be able to get the toplevel for the tip, but this
  //doesn't work
  CtkWidget  *toplevel;

  toplevel = ctk_widget_get_toplevel (parent);
  /*
  if (toplevel && ctk_widget_is_toplevel (toplevel) && CTK_IS_WINDOW (toplevel))
    ctk_window_set_transient_for (CTK_WINDOW (fixedtip), CTK_WINDOW (toplevel));
    */
#endif

  fixedtip->priv->orientation = orientation;

  //FIXME: would be nice to move the tip when the notification area moves
  g_signal_connect_object (parent, "size-allocate",
                           G_CALLBACK (na_fixed_tip_parent_size_allocated),
                           fixedtip, 0);
  g_signal_connect_object (parent, "screen-changed",
                           G_CALLBACK (na_fixed_tip_parent_screen_changed),
                           fixedtip, 0);

  na_fixed_tip_position (fixedtip);

  return CTK_WIDGET (fixedtip);
}

void
na_fixed_tip_set_markup (CtkWidget  *widget,
                         const char *markup_text)
{
  NaFixedTip *fixedtip;

  g_return_if_fail (NA_IS_FIXED_TIP (widget));

  fixedtip = NA_FIXED_TIP (widget);

  ctk_label_set_markup (CTK_LABEL (fixedtip->priv->label),
                        markup_text);

  na_fixed_tip_position (fixedtip);
}

void
na_fixed_tip_set_orientation (CtkWidget      *widget,
                              CtkOrientation  orientation)
{
  NaFixedTip *fixedtip;

  g_return_if_fail (NA_IS_FIXED_TIP (widget));

  fixedtip = NA_FIXED_TIP (widget);

  if (orientation == fixedtip->priv->orientation)
    return;

  fixedtip->priv->orientation = orientation;

  na_fixed_tip_position (fixedtip);
}
