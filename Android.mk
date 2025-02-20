ifeq ($(shell echo $(TARGET_BUILD_KERNEL_VERSION) | awk '{if ($$1 >= 5.15) print "true"}'),true)
include $(call all-subdir-makefiles)
endif