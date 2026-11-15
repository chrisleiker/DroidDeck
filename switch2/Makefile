.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libounce/switch2_rules

TARGET		:=	DroidDeck
BUILD		:=	build
SOURCES		:=	source
ROMFS		:=	romfs
ICON		:=	icon.jpg
APP_TITLE	:=	DroidDeck
APP_AUTHOR	:=	DroidDeck contributors
APP_VERSION	:=	0.7.1

ARCH	:=	$(SWITCH2_CPU) -mtp=soft -fPIE
CFLAGS	:=	-g -Wall -O2 -ffunction-sections $(ARCH) $(INCLUDE) -D__SWITCH__
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:=	-lSDL2_ttf -lSDL2_image -lSDL2 -lharfbuzz -lfreetype -lbz2 -lpng -ljpeg -lwebp -lz \
		-lEGL -lGLESv2 -lglapi -ldrm_nouveau -lvulkan -lkaihen -lnx -lstdc++ -lm
LIBDIRS	:=	$(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD	:=	$(CC)
export OFILES	:=	$(CFILES:.c=.o)
export INCLUDE	:=	$(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON	:=	$(TOPDIR)/$(ICON)
export NROFLAGS	+=	--icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp --romfsdir=$(CURDIR)/$(ROMFS)

.PHONY: $(BUILD) clean all host

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

host:
	@mkdir -p build-host
	cc -O2 -Wall -o build-host/droiddeck source/*.c $$(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image) -lm

clean:
	@rm -fr build build-host DroidDeck.nro DroidDeck.nacp DroidDeck.elf

else

DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT).nro
$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

endif
