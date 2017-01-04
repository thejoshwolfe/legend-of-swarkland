.PHONY: all
all:

PKG_CONFIG_DEPENDENCIES = SDL2_ttf sdl2
RESOURCE_NAMES = version_resource font_resource spritesheet_resource audio_attack_resource
OBJECT_NAMES = swarkland.o display.o load_image.o util.o thing.o path_finding.o map.o hashtable.o random.o decision.o serial.o byte_buffer.o item.o input.o event.o string.o text.o audio.o $(foreach f,$(RESOURCE_NAMES),$f.o)

CPP_FLAGS += $(TARGET_SPECIFIC_CPP_FLAGS) -fno-omit-frame-pointer -fno-exceptions -fno-rtti -Ibuild/native -Isrc -g -Wall -Wextra -Werror
COMPILE_CPP = $(CROSS_PREFIX)g++ -c -std=c++14 -o $@ -MMD -MP -MF $@.d -I$(dir $@) $(CPP_FLAGS) $(shell $(CROSS_PREFIX)pkg-config --cflags $(PKG_CONFIG_DEPENDENCIES)) $<

LINK_FLAGS += $(TARGET_SPECIFIC_LINK_FLAGS) -lm
LINK = $(CROSS_PREFIX)gcc -o $@ $^ $(LINK_FLAGS) $(shell $(CROSS_PREFIX)pkg-config --libs $(PKG_CONFIG_DEPENDENCIES))

# ld is not allowed to omit functions and global variables defined in .o files,
# but it can omit unused content from .a static libraries.
# first make a static library with ar, then make our binary out of that.
LINK_SPARSE = rm -f $@.a; $(foreach f,$^,$(CROSS_PREFIX)ar qcs $@.a $f;) $(CROSS_PREFIX)gcc -o $@ $@.a $(LINK_FLAGS)

THE_VERSION := $(shell echo $$(echo -n $$(cat version.txt)).$$(echo -n $$(git rev-parse --short HEAD)))
# this file has the full version number in its name.
# depend on this whenever you depend on the exact full version number.
VERSION_FILE = build/the_version_is.$(THE_VERSION)
$(VERSION_FILE): version.txt | build
	@rm -f build/the_version_is.*
	touch "$@"
build/full_version.txt: $(VERSION_FILE)
	echo -n "$(THE_VERSION)" > $@
%/version_resource: build/full_version.txt
	cp "$<" $@
%/font_resource: assets/font/DejaVuSansMono.ttf
	cp "$<" $@
%/audio_attack_resource: assets/audio/attack.raw
	cp "$<" $@
%/spritesheet_resource:
	PYTHONPATH=deps/simplepng.py/ ./tools/compile_spritesheet.py assets/img/ --glob='*.png' --tilesize=32 --spritesheet=$@ --header=$(dir $@)spritesheet.hpp --deps=$@.d
%/spritesheet.hpp: %/spritesheet_resource
	@# the spritesheet generation might update the .hpp

%_resource.o: %_resource
	@# the name of the file, e.g. "version_resource", is where these symbol names come from:
	@#   _binary_version_resource_start _binary_version_resource_end
	@# but wait, in windows, the leading underscore is not present, resulting in these names:
	@#    binary_version_resource_start  binary_version_resource_end
	cd $(dir $@) && $(CROSS_PREFIX)ld -r -b binary $(notdir $<) -o $(notdir $@)

.PHONY: native
all: native
-include $(wildcard build/native/*.d)
OBJECTS_native = $(foreach f,$(OBJECT_NAMES),build/native/$f)
RAW_RESOURCES_native = $(foreach f,$(RESOURCE_NAMES),build/native/$f)
$(OBJECTS_native) build/native/main.o: build/native/spritesheet.hpp
$(OBJECTS_native) build/native/main.o: TARGET_SPECIFIC_CPP_FLAGS = -fsanitize=address
$(OBJECTS_native) build/native/main.o: CROSS_PREFIX =
build/native/%.o: src/%.cpp
	$(COMPILE_CPP)

build/native/legend-of-swarkland: TARGET_SPECIFIC_LINK_FLAGS = -lasan
build/native/legend-of-swarkland: CROSS_PREFIX =
build/native/legend-of-swarkland: $(OBJECTS_native) build/native/main.o
	$(LINK)
native: build/native/legend-of-swarkland

$(OBJECTS_native) $(RAW_RESOURCES_native): | build/native
build/native:
	mkdir -p $@


# cross compiling for windows.
# See README.md for instructions.
.PHONY: windows
-include $(wildcard build/windows/*.d)
CROSS_windows = $(MXE_HOME)/usr/bin/i686-w64-mingw32.static-
OBJECTS_windows = $(foreach f,$(OBJECT_NAMES),build/windows/$f)
RAW_RESOURCES_windows= $(foreach f,$(RESOURCE_NAMES),build/windows/$f)
$(OBJECTS_windows) build/windows/main.o: build/windows/spritesheet.hpp
$(OBJECTS_windows) build/windows/main.o: TARGET_SPECIFIC_CPP_FLAGS =
$(OBJECTS_windows) build/windows/main.o: CROSS_PREFIX = $(CROSS_windows)
build/windows/%.o: src/%.cpp
	$(if $(MXE_HOME),,$(error MXE_HOME is not defined))
	$(COMPILE_CPP)

build/windows/legend-of-swarkland.exe: TARGET_SPECIFIC_LINK_FLAGS = -static -mconsole
build/windows/legend-of-swarkland.exe: CROSS_PREFIX = $(CROSS_windows)
build/windows/legend-of-swarkland.exe: $(OBJECTS_windows) build/windows/main.o
	$(LINK)
windows: build/windows/legend-of-swarkland.exe

$(OBJECTS_windows) $(RAW_RESOURCES_windows): | build/windows
build/windows:
	mkdir -p $@


.PHONY: publish-windows
PUBLISH_EXE_NAME = legend-of-swarkland-$(THE_VERSION).exe
PUBLISH_DIR = build/publish
FULL_PUBLISH_EXE_NAME = $(PUBLISH_DIR)/$(PUBLISH_EXE_NAME)
publish-windows: $(FULL_PUBLISH_EXE_NAME)

$(FULL_PUBLISH_EXE_NAME): build/windows/legend-of-swarkland.exe
	rm -rf $(PUBLISH_DIR) && mkdir $(PUBLISH_DIR)
	cp $< $@.tmp.exe
	upx $@.tmp.exe
	mv $@.tmp.exe $@
	touch $@

build/publish-windows:
	mkdir -p $@

.PHONY: test
TEST_SOURCES = $(wildcard test/*.swarkland)
TEST_OUTPUTS = $(patsubst test/%.swarkland,build/test/%.timestamp,$(TEST_SOURCES))
test: $(TEST_OUTPUTS)
$(TEST_OUTPUTS): build/native/legend-of-swarkland | build/test
build/test/%.timestamp: test/%.swarkland
	./build/native/legend-of-swarkland --headless $<
	@touch $@

build/test:
	mkdir -p $@

all: test

build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build
