
ccflags-y += -D__FF_BUILD_DATE=\"$(shell date +%Y%m%d)\"

# Early-logging level for ff_hal, ff_ta and driver as well. Note that this
# logging level only takes effect before ff_config module is loaded.
#
# Options:
# 0 - FF_LOG_LEVEL_ALL, All
# 1 - FF_LOG_LEVEL_VBS, Verbose
# 2 - FF_LOG_LEVEL_DBG, Debug
# 3 - FF_LOG_LEVEL_INF, Info
# 4 - FF_LOG_LEVEL_WRN, Warning
# 5 - FF_LOG_LEVEL_ERR, Error
ccflags-y += -D__FF_EARLY_LOG_LEVEL=2
#subdir-ccflags-y += -I$(srctree)/drivers/ontim/include/
 
focaltech_fp-y := ff_ctl.o
focaltech_fp-y += plat-sdm660.o
focaltech-y := focaltech_fp.o
obj-$(CONFIG_QCOM_FOCALTECH_FINGERPRINT) += focaltech.o
ifeq ($(CONFIG_QCOM_FOCALTECH_FINGERPRINT),m)
$(info "--------focaltech IS BUILT AS DLKM")
BOARD_VENDOR_KERNEL_MODULES +=  $(KERNEL_MODULES_OUT)/focaltech.ko
endif
