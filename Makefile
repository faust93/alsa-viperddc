PREFIX = /usr
LIBDIR = $(PREFIX)/lib/alsa-lib

CFLAGS = -O3 -fPIC -DPIC
CFLAGS += -Wall -Wextra -Wpedantic -Wno-unused-parameter

LDFLAGS=-lasound
INSTALL=/usr/bin/install

all: libasound_module_pcm_ddc.so

libasound_module_pcm_ddc.so: pcm_ddc.c vdc.h
	$(CC) -o $@ -shared ${CFLAGS} pcm_ddc.c ${LDFLAGS}

install: libasound_module_pcm_ddc.so
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 644 libasound_module_pcm_ddc.so $(LIBDIR)/

uninstall:
	rm -f $(LIBDIR)/libasound_module_pcm_ddc.so

clean:
	rm -f libasound_module_pcm_ddc.so *~
