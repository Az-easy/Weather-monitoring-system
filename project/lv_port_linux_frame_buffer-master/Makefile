#
# Makefile
#-L/home/gec/freetype-2.12.1/tmp/lib -I/home/gec/freetype-2.12.1/tmp/include/freetype2
CC = arm-linux-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
CFLAGS ?= -O3 -g0   -I$(LVGL_DIR)/ -Wall  -std=gnu99 \
          -Isrc/ui -Isrc/network -Isrc/hardware -Isrc/common -Isrc/lib -Isrc/fonts
LDFLAGS ?= -lm -lpthread
BIN = demo


#Collect the files to compile
MAINSRC = ./main.c
TESTSRC = $(wildcard ./src/ui/*.c ./src/network/*.c ./src/hardware/*.c \
                     ./src/common/*.c ./src/lib/*.c ./src/fonts/*.c) ##1. 添加新的源文件

USRCODESRC = ${shell find $(LVGL_DIR)/usrcode -name '*.c'}
include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS +=$(LVGL_DIR)/mouse_cursor_icon.c 

OBJEXT ?= .o
# 所有 .o 统一放到新建的 obj/ 目录下,按源码目录结构存放,避免同名文件冲突
OBJDIR = obj

# 源文件路径有 ./ 开头(main.c、src/)和 $(LVGL_DIR)/ 开头(lvgl、lv_drivers、usrcode)两种,
# 先去掉公共前缀得到干净相对路径,再映射到 $(OBJDIR)/ 下
REL_CSRCS   = $(patsubst ./%,%,$(patsubst $(LVGL_DIR)/%,%,$(CSRCS)))
REL_ASRCS   = $(patsubst ./%,%,$(patsubst $(LVGL_DIR)/%,%,$(ASRCS)))
REL_MAINSRC = $(patsubst ./%,%,$(patsubst $(LVGL_DIR)/%,%,$(MAINSRC)))
REL_TESTSRC = $(patsubst ./%,%,$(patsubst $(LVGL_DIR)/%,%,$(TESTSRC)))
REL_USRCODE = $(patsubst ./%,%,$(patsubst $(LVGL_DIR)/%,%,$(USRCODESRC)))

AOBJS      = $(patsubst %.S,$(OBJDIR)/%.o,$(REL_ASRCS))
COBJS      = $(patsubst %.c,$(OBJDIR)/%.o,$(REL_CSRCS))
MAINOBJ    = $(patsubst %.c,$(OBJDIR)/%.o,$(REL_MAINSRC))
TESTOBJ    = $(patsubst %.c,$(OBJDIR)/%.o,$(REL_TESTSRC)) ##2. 添加.o文件
USRCODEOBJ = $(patsubst %.c,$(OBJDIR)/%.o,$(REL_USRCODE))  #添加组件

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC) $(USRCODESRC)
OBJS = $(AOBJS) $(COBJS)

## MAINOBJ -> OBJFILES


all: default

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

default: $(AOBJS) $(COBJS) $(MAINOBJ) $(TESTOBJ) $(USRCODEOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(TESTOBJ) $(AOBJS) $(COBJS) $(LDFLAGS) $(USRCODEOBJ)

clean:
	rm -f $(BIN)
	rm -rf $(OBJDIR)

