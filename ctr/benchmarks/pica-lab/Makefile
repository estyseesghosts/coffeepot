# Minimal adaptation of devkitPro/3ds-examples application Makefile.
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; install devkitPro 3DS development tools)
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules
TARGET := pica-lab
BUILD := build
SOURCES := source
INCLUDES :=
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB) $(DEVKITPRO)/portlibs/3ds
APP_TITLE := PICA Lab
APP_DESCRIPTION := Reproducible PICA200 research
APP_AUTHOR := Community research
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
export LD := $(CC)
export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES_BIN := $(PICAFILES:.v.pica=.shbin.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(PICAFILES:.v.pica=_shbin.h)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $@
clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf
else
$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES): $(HFILES)
$(OUTPUT).elf: $(OFILES)
.PRECIOUS: %.shbin
%.shbin.o %_shbin.h: %.shbin
	$(SILENTMSG) $(notdir $<)
	$(bin2o)
-include $(DEPSDIR)/*.d
endif
