MODULE_NAME = aml_dhp
obj-$(CONFIG_AMLOGIC_MEDIA_DHP) += ${MODULE_NAME}.o
${MODULE_NAME}-objs += aml_dhp_drv.o

PR_FMT = "DHP"
PR_FMT_DEFINE="-Dpr_fmt(fmt)=\"[$(PR_FMT)]:\" fmt"
ccflags-y += $(PR_FMT_DEFINE)
