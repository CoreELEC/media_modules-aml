OUT_DIR ?= .
$(info "OUT_DIR : $(OUT_DIR)")

TARGET := esplayer dec_slt DecInfo_test vdec_debug

ESPLAYER_OBJS := esplayer.o	vcodec.o
DEC_SLT_OBJS := dec_slt.o vcodec.o
DECINFO_TEST_OBJS := DecInfo_test.o
VDEC_DEBUG_OBJS := vdec_debug_port.o

#LOCAL_PATH=$(CURDIR)
#CFLAGS = -I $(LOCAL_PATH)/../../drivers/amvdec_ports/decoder
#CFLAGS += -Wformat -Wformat-security -Werror=format-security
#CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch -Os
CFLAGS += -Os
LDFLAGS += -Os

all: $(TARGET)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $(OUT_DIR)/$@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $(OUT_DIR)/$@

esplayer: $(ESPLAYER_OBJS)
	$(CXX) $(LDFLAGS) $(patsubst %,$(OUT_DIR)/%,$^) -o $(OUT_DIR)/$@

dec_slt: $(DEC_SLT_OBJS)
	$(CXX) $(LDFLAGS) $(patsubst %,$(OUT_DIR)/%,$^) -o $(OUT_DIR)/$@

DecInfo_test: $(DECINFO_TEST_OBJS)
	$(CXX) $(LDFLAGS) $(patsubst %,$(OUT_DIR)/%,$^) -o $(OUT_DIR)/$@

vdec_debug: $(VDEC_DEBUG_OBJS)
	$(CXX) $(LDFLAGS) $(patsubst %,$(OUT_DIR)/%,$^) -o $(OUT_DIR)/$@

install:
	install -p -m 755 $(OUT_DIR)/esplayer $(TARGET_DIR)/usr/bin
	install -p -m 755  $(OUT_DIR)/dec_slt $(TARGET_DIR)/usr/bin
	install -p -m 755  $(OUT_DIR)/DecInfo_test $(TARGET_DIR)/usr/bin
	install -p -m 755  $(OUT_DIR)/vdec_debug $(TARGET_DIR)/usr/bin

clean:
	rm -rf $(patsubst %,$(OUT_DIR)/%,${TARGET}) $(OUT_DIR)/*.o

.PHONY:clean

$(shell mkdir -p $(OUT_DIR))