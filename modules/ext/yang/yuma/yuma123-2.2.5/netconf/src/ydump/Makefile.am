lib_LTLIBRARIES = libydump.la

libydump_la_SOURCES = \
$(top_srcdir)/netconf/src/ydump/c.c \
$(top_srcdir)/netconf/src/ydump/c_util.c \
$(top_srcdir)/netconf/src/ydump/cyang.c \
$(top_srcdir)/netconf/src/ydump/h.c \
$(top_srcdir)/netconf/src/ydump/html.c \
$(top_srcdir)/netconf/src/ydump/py_util.c \
$(top_srcdir)/netconf/src/ydump/sql.c \
$(top_srcdir)/netconf/src/ydump/tg2.c \
$(top_srcdir)/netconf/src/ydump/xsd.c \
$(top_srcdir)/netconf/src/ydump/xsd_typ.c \
$(top_srcdir)/netconf/src/ydump/xsd_util.c \
$(top_srcdir)/netconf/src/ydump/xsd_yang.c \
$(top_srcdir)/netconf/src/ydump/yangdump_util.c \
$(top_srcdir)/netconf/src/ydump/yangstats.c \
$(top_srcdir)/netconf/src/ydump/yangyin.c \
$(top_srcdir)/netconf/src/ydump/ydump.c

libydump_la_CPPFLAGS = -I$(top_srcdir)/netconf/src/ydump/ -I$(top_srcdir)/netconf/src/ncx -I$(top_srcdir)/netconf/src/platform -I/usr/include/libxml2 -I/usr/include/libxml2/libxml
libydump_la_LDFLAGS = -version-info 2:0 -lxml2 -lz  -ldl



