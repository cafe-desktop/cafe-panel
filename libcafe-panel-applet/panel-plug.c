/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include <ctk/ctk.h>

#include "panel-plug-private.h"

struct _PanelPlug
{
  CtkPlug parent;
};

G_DEFINE_TYPE (PanelPlug, panel_plug, CTK_TYPE_PLUG)

static gboolean
panel_plug_draw (CtkWidget *widget,
                 cairo_t   *cr)
{
  CdkWindow *window;
  cairo_pattern_t *pattern;

  if (!ctk_widget_get_realized (widget))
    return CTK_WIDGET_CLASS (panel_plug_parent_class)->draw (widget, cr);

  window = ctk_widget_get_window (widget);
  pattern = cdk_window_get_background_pattern (window);

  if (!pattern)
    {
      CtkStyleContext *context;
      gint width;
      gint height;

      context = ctk_widget_get_style_context (widget);
      width = ctk_widget_get_allocated_width (widget);
      height = ctk_widget_get_allocated_height (widget);

      ctk_render_background (context, cr, 0, 0, width, height);
    }

  return CTK_WIDGET_CLASS (panel_plug_parent_class)->draw (widget, cr);
}

static void
panel_plug_realize (CtkWidget *widget)
{
  CdkScreen *screen;
  CdkVisual *visual;

  screen = cdk_screen_get_default ();
  visual = cdk_screen_get_rgba_visual (screen);

  if (!visual)
    visual = cdk_screen_get_system_visual (screen);

  ctk_widget_set_visual (widget, visual);

  CTK_WIDGET_CLASS (panel_plug_parent_class)->realize (widget);
}

static void
panel_plug_class_init (PanelPlugClass *plug_class)
{
  CtkWidgetClass *widget_class;

  widget_class = CTK_WIDGET_CLASS (plug_class);

  widget_class->draw = panel_plug_draw;
  widget_class->realize = panel_plug_realize;

  ctk_widget_class_set_css_name (widget_class, "PanelApplet");
}

static void
panel_plug_init (PanelPlug *plug)
{
  ctk_widget_set_app_paintable (CTK_WIDGET (plug), TRUE);
}

CtkWidget *
panel_plug_new (void)
{
  return g_object_new (PANEL_TYPE_PLUG, NULL);
}
