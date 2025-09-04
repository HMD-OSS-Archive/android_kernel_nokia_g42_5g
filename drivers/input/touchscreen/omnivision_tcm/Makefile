#
# Makefile for the Omnivision TCM touchscreen driver.
#

# Each configuration option enables a list of files.
ifneq ($(TARGET_BUILD_VARIANT),user)
    $(info ++++++++++++++++++touch_gki++++++++++++++++$(TARGET_BUILD_VARIANT))
    ccflags-y += -DGKI_FLAG_EN
endif
subdir-ccflags-y += -I$(srctree)/drivers/ontim/include/
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM) += omnivision_tcm.o
omnivision_tcm-y := omnivision_tcm_core.o \
	omnivision_tcm_touch.o omnivision_tcm_spi.o \
	omnivision_tcm_zeroflash.o \
	omnivision_tcm_reflash.o \
	omnivision_tcm_device.o \
	omnivision_tcm_testing.o \
	omnivision_tcm_csv_parser.o \
	omnivision_tcm_recovery.o \
	omnivision_tcm_diagnostics.o

ifeq ($(CONFIG_TOUCHSCREEN_OMNIVISION_TCM),m)
$(info "omnivision TD4160 IS BUILT AS DLKM")
BOARD_VENDOR_KERNEL_MODULES +=  $(KERNEL_MODULES_OUT)/omnivision_tcm.ko
endif
