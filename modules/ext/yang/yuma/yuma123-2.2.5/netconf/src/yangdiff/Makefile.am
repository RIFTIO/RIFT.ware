bin_PROGRAMS = yangdiff

yangdiff_SOURCES = \
$(top_srcdir)/netconf/src/yangdiff/yangdiff.c \
$(top_srcdir)/netconf/src/yangdiff/yangdiff_grp.c \
$(top_srcdir)/netconf/src/yangdiff/yangdiff_obj.c \
$(top_srcdir)/netconf/src/yangdiff/yangdiff_typ.c \
$(top_srcdir)/netconf/src/yangdiff/yangdiff_util.c

yangdiff_CPPFLAGS = -I$(top_srcdir)/netconf/src/ydump/ -I$(top_srcdir)/netconf/src/ncx -I$(top_srcdir)/netconf/src/platform -I$(top_srcdir)/netconf/src/ydump -I/usr/include/libxml2 -I/usr/include/libxml2/libxml

yangdiff_LDFLAGS = $(top_builddir)/netconf/src/ydump/libydump.la $(top_builddir)/netconf/src/ncx/libncx.la -lxml2 -lz  -ldl

