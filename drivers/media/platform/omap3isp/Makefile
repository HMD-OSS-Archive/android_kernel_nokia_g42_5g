# SPDX-License-Identifier: GPL-2.0
# Makefile for OMAP3 ISP driver

#guozuoyu add switch for camera 20220906 start
ifeq ($(TARGET_DTS_CONFIG), CrusaderTF)
KBUILD_CFLAGS += -DCAM_ERR_CODE
$(info clk KBUILD_CFLAGS CAM_ERR_CODE)
endif
#guozuoyu add switch for camera 20220906 end

ccflags-$(CONFIG_VIDEO_OMAP3_DEBUG) += -DDEBUG

omap3-isp-objs += \
	isp.o ispvideo.o \
	ispcsiphy.o ispccp2.o ispcsi2.o \
	ispccdc.o isppreview.o ispresizer.o \
	ispstat.o isph3a_aewb.o isph3a_af.o isphist.o

obj-$(CONFIG_VIDEO_OMAP3) += omap3-isp.o
