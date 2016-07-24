bin_PROGRAMS = yangdump

yangdump_SOURCES = \
$(top_srcdir)/netconf/src/yangdump/yangdump.c

yangdump_CPPFLAGS = -I$(top_srcdir)/netconf/src/ydump/ -I$(top_srcdir)/netconf/src/ncx -I$(top_srcdir)/netconf/src/platform -I$(top_srcdir)/netconf/src/ydump -I/usr/include/libxml2 -I/usr/include/libxml2/libxml

yangdump_LDFLAGS = $(top_builddir)/netconf/src/ydump/libydump.la $(top_builddir)/netconf/src/ncx/libncx.la -lxml2 -lz  -ldl

