
# Toyp makefile

# We attempt to build from the RiftWare install directory, only
RIFT_INSTALL=/net/strange/localdisk/tseidenb/ws-strange/fix-1/rift/.install

CFLAGS += -g -O1 -Wall -Werror

CPPFLAGS += -I$(RIFT_INSTALL)/usr/include
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/riftware
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/glib-2.0
CPPFLAGS += -I$(RIFT_INSTALL)/usr/lib/glib-2.0/include
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/libpeas-1.0
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/gobject-introspection-1.0
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/dispatch
CPPFLAGS += -I$(RIFT_INSTALL)/usr/include/rift/plugins

# Giant gobs of libraries
RWCAL_LIBS = -lrwcal_api -lrwcal-1.0 -lrwcal_yang_gen -lrwplugin-1.0
RWVCS_LIBS = -lrwtasklet -lrwvcs -lrwmemlog_yang_gen -lrwmanifest_yang_gen -lrw_vcs_yang_gen -lrwnetns -lrw_vx_plugin -lrwpython_util-1.0 -lrwlog-mgmt_yang_gen -lpeas-1.0 -lgobject-2.0
RWSCHED_LIBS = -lrwsched -lrwlib -ltalloc -lCoreFoundation -ldispatch -lBlocksRuntime -lck -lglib-2.0 -lpthread_workqueue -lkqueue
RWMSG_LIBS = -lrwmsg -lrwtrace -lrwlog -lnanomsg -lprotobuf-c
RWDTS_LIBS = -lrwdts_api -lrwdtsproto -lrwmemlog -lrwdts_redis -lkv_light_api -lrwdts_yang_gen
RWYANG_LIBS = -lrwyang -lrw_schema_pb -lrwschema_yang_gen -lrwext_yang_gen -lietf_yang_gen -lrwtypes -lncx


# Generally, one or two yang_gen libraries for each foo.ypbc.h used in the code
SCHEMA_LIBS =-lrwuagent_yang_gen -lrwlog-mgmt_yang_gen

LDLIBS += $(RWDTS_LIBS) $(RWYANG_LIBS) $(RWMSG_LIBS) $(RWSCHED_LIBS) $(RWVCS_LIBS) $(RWCAL_LIBS) $(SCHEMA_LIBS)

LDFLAGS += -L$(RIFT_INSTALL)/usr/lib
LDFLAGS += -L$(RIFT_INSTALL)/usr/lib/rift/plugins

toyp: toyp.o
	gcc -o $@ $< $(CFLAGS) $(LDFLAGS) $(LDLIBS)

toyp.o: toyp.c
	gcc -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f toyp *~ toyp.o
