# SPDX-License-Identifier: GPL-2.0-only
$(warning "Compile HMD_DISABLE_UART $(HMD_DISABLE_UART)")


obj-y	= printk.o
obj-$(CONFIG_PRINTK)	+= printk_safe.o
obj-$(CONFIG_A11Y_BRAILLE_CONSOLE)	+= braille.o


ifneq ($(HMD_DISABLE_UART),true)
subdir-ccflags-y += -DCONFIG_PRINTK_HMD_PREFIX
subdir-ccflags-y += -DCONFIG_HMD_PRINTK_UART_CONSOLE
subdir-ccflags-y += -DCONFIG_HMD_PRINTK_UART_CONSOLE_DEFAULT_Y
endif
