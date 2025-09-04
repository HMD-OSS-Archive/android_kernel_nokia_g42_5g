ifeq ($(BUILD_SMT),true)
    ccflags-y += -DSMT_VERSION
    $(info ++++++++++++++++++++++++++firefly++++++++++++++++ SMT version)
endif

obj-$(CONFIG_ONTIM_HWINFO) += gpio_boardid.o
obj-$(CONFIG_ONTIM_HWINFO) += hwinfo.o
