MODULE_NAME = encoder_common
obj-$(CONFIG_AMLOGIC_MEDIA_VENC_COMMON)	+=	$(MODULE_NAME).o

$(MODULE_NAME)-objs	+=	encoder_report.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)=\"[$(PR_FMT)]:\" fmt"
ccflags-y += $(PR_FMT_DEFINE)
