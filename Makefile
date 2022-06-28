mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
MEDIA_MODULE_PATH := $(dir $(mkfile_path))
VERSION_CONTROL_CFLAGS := $(shell ${MEDIA_MODULE_PATH}/version_control.sh)

CONFIGS := CONFIG_AMLOGIC_MEDIA_VDEC_MPEG12=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG2_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG4=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MPEG4_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_VC1=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H264_MVC=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_H265=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_VP9=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MJPEG=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_MJPEG_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_REAL=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AVS=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AVS_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AVS2=m \
	CONFIG_AMLOGIC_MEDIA_VENC_H264=m \
	CONFIG_AMLOGIC_MEDIA_VENC_H265=m \
	CONFIG_AMLOGIC_MEDIA_VDEC_AV1=m \
	CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION=y \
	CONFIG_AMLOGIC_MEDIA_GE2D=y \
	CONFIG_AMLOGIC_MEDIA_VENC_MULTI=m \
	CONFIG_AMLOGIC_MEDIA_VENC_JPEG=m

EXTRA_INCLUDE := -I$(KERNEL_SRC)/$(M)/drivers/include

CONFIGS_BUILD := -Wno-parentheses-equality -Wno-pointer-bool-conversion \
				-Wno-unused-const-variable -Wno-typedef-redefinition \
				-Wno-logical-not-parentheses -Wno-sometimes-uninitialized \
				-Wno-frame-larger-than=

ifeq ($(CLANG_VERSION), r450784d)
CONFIGS_BUILD += -Wno-bitwise-instead-of-logical
endif

KBUILD_CFLAGS_MODULE += $(GKI_EXT_MODULE_PREDEFINE)

modules:
	$(MAKE) -C  $(KERNEL_SRC) M=$(M)/drivers modules "EXTRA_CFLAGS+=-I$(INCLUDE) -Wno-error $(CONFIGS_BUILD) $(EXTRA_INCLUDE) $(KBUILD_CFLAGS_MODULE) ${VERSION_CONTROL_CFLAGS}" $(CONFIGS)

all: modules

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 M=$(M)/drivers -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;
	mkdir -p ${OUT_DIR}/../vendor_lib/firmware/video
	cp $(KERNEL_SRC)/$(M)/firmware/* ${OUT_DIR}/../vendor_lib/firmware/video -rf

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
