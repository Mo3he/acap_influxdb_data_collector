PROG1 = acap_influxdb
OBJS1 = main.c ACAP.c cJSON.c influxdb.c
PROGS = $(PROG1)

PKGS = glib-2.0 gio-2.0 vdostream axevent fcgi
# -std=gnu11 keeps GCC 13 (SDK 12.10 / Ubuntu 24.04) from redirecting atoi/strtol/sscanf
# to C23 __isoc23_* symbols (GLIBC_2.38), which would break install on AXIS OS 10/11.
CFLAGS += -std=gnu11
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS += -s -lm -ldl -laxparameter -lfcgi -lcurl

all: $(PROGS)

$(PROG1): $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(PROGS) *.o $(LIBDIR) *.eap* *_LICENSE.txt manifest.json package.conf* param.conf
