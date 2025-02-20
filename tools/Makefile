OUT_DIR ?= .
$(info "OUT_DIR : $(OUT_DIR)")

SUBDIRS = player v4lplayer

EXTRA_CFLAGS := OUT_DIR=${OUT_DIR} TARGET_DIR=${TARGET_DIR} STAGING_DIR=${STAGING_DIR}

all:
	$(MAKE) -C player ${EXTRA_CFLAGS} all
	$(MAKE) -C v4lplayer ${EXTRA_CFLAGS} all

install:
	$(MAKE) -C player ${EXTRA_CFLAGS} install
	$(MAKE) -C v4lplayer ${EXTRA_CFLAGS} install

clean:
	$(MAKE) -C player ${EXTRA_CFLAGS} clean
	$(MAKE) -C v4lplayer ${EXTRA_CFLAGS} clean

.PHONY: all install clean

$(shell mkdir -p $(SUBDIRS))