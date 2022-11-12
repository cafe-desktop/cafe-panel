/*
 * libvnck based tasklist Applet.
 * (C) 2001 Alexander Larsson
 *
 * Authors: Alexander Larsson
 *
 */

#ifndef __WINDOW_LIST_H__
#define __WINDOW_LIST_H__

#include <glib.h>
#include <cafe-panel-applet.h>

#ifdef __cplusplus
extern "C" {
#endif

gboolean window_list_applet_fill(CafePanelApplet* applet);

#ifdef __cplusplus
}
#endif

#endif
