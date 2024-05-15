MODULE_NAME = encoder
obj-$(CONFIG_AMLOGIC_MEDIA_VENC_H264) = $(MODULE_NAME).o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)
