LV_DRIVERS_DIR_NAME ?= lv_drivers

override CFLAGS := -I$(LVGL_DIR) $(CFLAGS)

CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/*.c)
# 本项目只用 fbdev/evdev,其他驱动目录已删除,不再编译
# CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/wayland/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/indev/*.c)
# CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/gtkdrv/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/display/*.c)
# CSRCS += $(wildcard $(LVGL_DIR)/$(LV_DRIVERS_DIR_NAME)/sdl/*.c)
