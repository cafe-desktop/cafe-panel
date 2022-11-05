## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module, e.g. 'glib'.
DOC_MODULE=cafe-panel-applet

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# ctk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../../../libcafe-panel-applet

# Extra options to supply to ctkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to ctkdoc-mkdb.
MKDB_OPTIONS=--output-format=xml

# Extra options to supply to ctkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=$(top_srcdir)/libcafe-panel-applet/*.h
CFILE_GLOB=$(top_srcdir)/libcafe-panel-applet/*.c

# Header files to ignore when scanning.
IGNORE_HFILES =					\
	CAFE_Panel.h				\
	cafe-panel-applet-enums.h			\
	cafe-panel-applet-marshal.h			\
	cafe-panel-applet-private.h			\
	cafe-panel-applet-shell.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# Other files to distribute.
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed if your app/lib
# contains GtkObjects/GObjects and you want to document signals and properties.
GTKDOC_CFLAGS =					\
	$(CFLAGS)				\
	$(CPPFLAGS)				\
	$(PANEL_CFLAGS)				\
	-I$(srcdir)/../../../libcafe-panel-applet    \
	-I$(top_builddir)/libcafe-panel-applet

GTKDOC_LIBS =						\
	$(LDFLAGS)					\
	$(PANEL_LIBS)					\
	../../../libcafe-panel-applet/libcafe-panel-applet-4.la

include $(top_srcdir)/ctk-doc.make

dist-hook-local:
#	mkdir $(distdir)/TEXT;          \
#	for f in $(srcdir)/TEXT/* ; do  \
#	  test -f $$f && cp -p $$f $(distdir)/TEXT; \
#	done

-include $(top_srcdir)/git.mk
