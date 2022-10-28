SUBDIRS = \
	theme

layoutdir   = $(datadir)/cafe-panel/layouts
layout_DATA = \
	default.layout \
	fedora.layout \
	linuxmint.layout \
	mageia.layout \
	opensuse.layout \
	ubuntu.layout

gsettings_ENUM_NAMESPACE = org.cafe.panel
gsettings_ENUM_FILES = $(top_srcdir)/cafe-panel/panel-enums-gsettings.h

panel_gschemas_in = \
	org.cafe.panel.gschema.xml.in \
	org.cafe.panel.object.gschema.xml.in \
	org.cafe.panel.toplevel.gschema.xml.in \
	org.cafe.panel.menubar.gschema.xml.in

gsettings_SCHEMAS = $(panel_gschemas_in:.xml.in=.xml)

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(panel_gschemas_in) \
	$(layout_DATA)

CLEANFILES = $(gsettings_SCHEMAS)
