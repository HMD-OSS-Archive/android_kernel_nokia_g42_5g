obj-$(CONFIG_HAPTIC_NV)	+=  haptic.o
haptic-objs = aw862xx.o haptic_nv.o

ifeq ($(BUILD_USER_TYPE_VERSION),true)
subdir-ccflags-y += -DBUILD_USER_TYPE_VERSION
endif
