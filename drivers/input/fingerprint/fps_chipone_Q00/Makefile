#obj-$(CONFIG_QCOM_CHIPONE_FINGERPRINT)    += fpsensor_spi_tee.o
#subdir-ccflags-y += -I$(srctree)/drivers/ontim/include/

fpsensor-y := fpsensor_spi_tee.o
obj-$(CONFIG_QCOM_CHIPONE_FINGERPRINT) += fpsensor.o
ifeq ($(CONFIG_QCOM_CHIPONE_FINGERPRINT),m)
$(info "--------fpsensor IS BUILT AS DLKM")
BOARD_VENDOR_KERNEL_MODULES +=  $(KERNEL_MODULES_OUT)/fpsensor.ko
endif
