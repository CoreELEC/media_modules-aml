OUT_DIR ?= .
$(info "OUT_DIR : $(OUT_DIR)")

TARGET := v4lplayer

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SRCS))

#LOCAL_PATH=$(CURDIR)
#CFLAGS = -I $(LOCAL_PATH)/../../drivers/amvdec_ports/decoder
#CFLAGS += -Wformat -Wformat-security -Werror=format-security
#CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch -Os
CFLAGS += -Os
LDFLAGS += -Os

all: $(TARGET)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $(OUT_DIR)/$@

v4lplayer: $(OBJS)
	$(CXX) $(LDFLAGS) $(patsubst %,$(OUT_DIR)/%,$^) -o $(OUT_DIR)/$@

install:
	install -p -m 755 $(OUT_DIR)/v4lplayer $(TARGET_DIR)/usr/bin

clean:
	rm -rf $(patsubst %,$(OUT_DIR)/%,${TARGET}) $(OUT_DIR)/*.o

.PHONY:clean

$(shell mkdir -p $(OUT_DIR))