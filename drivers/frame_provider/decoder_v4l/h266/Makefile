MODULE_NAME = amvdec_h266_v4l
obj-$(CONFIG_AMLOGIC_MEDIA_VDEC_H266) += ${MODULE_NAME}.o
${MODULE_NAME}-objs += vh266.o

PR_FMT = $(subst amvdec_,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)=\"[$(PR_FMT)]:\" fmt"
ccflags-y += $(PR_FMT_DEFINE)
