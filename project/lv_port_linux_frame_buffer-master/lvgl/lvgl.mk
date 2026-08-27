LVGL_PATH ?= ${shell pwd}/lvgl

CSRCS += $(shell find $(LVGL_PATH)/src -type f -name '*.c')
# 本项目只用 lvgl/src,官方 demos/examples 已删除,不再编译
# CSRCS += $(shell find $(LVGL_PATH)/demos -type f -name '*.c')
# CSRCS += $(shell find $(LVGL_PATH)/examples -type f -name '*.c')
CFLAGS += "-I$(LVGL_PATH)"
