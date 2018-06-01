SERVICE := helpline
DESTDIR ?= dist_root/
CHECKERDIR ?= checker_root/
SERVICEDIR ?= /srv/$(SERVICE)
#MAKEFLAGS = -j9

.PHONY: build install build_checker install_checker

build:
	cd yate/ ; CFLAGS="-g -O2" ./configure --prefix=$(shell pwd)/dist_root/$(SERVICEDIR) --without-mysql --without-libgsm --without-libspeex --without-openssl --without-libqt4
	$(MAKE) $(MAKEFLAGS) -C yate/

install: build
	install -d $(DESTDIR)$(SERVICEDIR)
	$(MAKE) -C yate/ install
	rm $(DESTDIR)$(SERVICEDIR)/etc/yate/*.conf
	install conf.d-service/*.conf $(DESTDIR)$(SERVICEDIR)/etc/yate/
	install -d $(DESTDIR)$(SERVICEDIR)/share/yate/sounds/helpline/
	install sounds-IVR/*.mulaw $(DESTDIR)$(SERVICEDIR)/share/yate/sounds/helpline/
	patchelf --set-rpath $(SERVICEDIR)/lib/ $(DESTDIR)$(SERVICEDIR)/bin/yate

build_checker:
	$(MAKE) -C yate/ clean
	cd yate/ ; CFLAGS="-g -DCHECKER -O2" ./configure --prefix=$(shell pwd)/checker_root/helpline/ --without-mysql --without-libgsm --without-libspeex --without-openssl --without-libqt4
	$(MAKE) $(MAKEFLAGS) -C yate/

install_checker: build_checker
	install -d $(CHECKERDIR)/helpline
	$(MAKE) -C yate/ install
	rm $(CHECKERDIR)/helpline/etc/yate/*.conf
	install conf.d-checker/*.conf $(CHECKERDIR)/helpline/etc/yate/
	install -d $(CHECKERDIR)/helpline/share/yate/sounds/helpline/
	patchelf --set-rpath /srv/helpline/lib/ $(CHECKERDIR)/helpline/bin/yate
