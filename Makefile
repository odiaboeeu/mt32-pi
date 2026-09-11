#
# Makefile
#

include Config.mk

.DEFAULT_GOAL=all
.PHONY: submodules circle-stdlib mt32emu fluidsynth switch-board check-circle-board sc55-core all clean veryclean

#
# Functions to apply/reverse patches only if not completely applied/reversed already
#
APPLY_PATCH=sh -c '																\
	if patch --dry-run --reverse --force --strip 1 --directory $$1 < $$2 >/dev/null 2>&1; then						\
		echo "Patch $$2 already applied; skipping";											\
	else																	\
		echo "Applying patch $$2 to directory $$1...";											\
		patch --forward --strip 1 --no-backup-if-mismatch -r - --directory $$1 < $$2 || (echo "Patch $$2 failed" >&2 && exit 1);	\
	fi' APPLY_PATCH

REVERSE_PATCH=sh -c '																\
	if patch --dry-run --forward --force --strip 1 --directory $$1 < $$2 >/dev/null 2>&1; then						\
		echo "Patch $$2 already reversed; skipping";											\
	else																	\
		echo "Reversing patch $$2 in directory $$1...";											\
		patch --reverse --strip 1 --no-backup-if-mismatch -r - --directory $$1 < $$2 || (echo "Patch $$2 failed" >&2 && exit 1);	\
	fi' REVERSE_PATCH

#
# Get submodules
#
submodules:
	@git submodule update --init --depth 1
	@git -C external/circle-stdlib submodule update --init --depth 1 libs/circle libs/circle-newlib
	@git -C external/circle-stdlib/libs/circle submodule update --init --depth 1 addon/wlan/hostap

#
# Configure circle-stdlib
#
$(CIRCLE_STDLIB_CONFIG) $(CIRCLE_CONFIG)&:
	@echo "Configuring for Raspberry Pi $(RASPBERRYPI) ($(BITS) bit)"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=$(RASPBERRYPI) --prefix=$(PREFIX)

# Apply patches

ifeq ($(strip $(GC_SECTIONS)),1)
# Enable function/data sections for circle-stdlib
	@echo "CFLAGS_FOR_TARGET += -ffunction-sections -fdata-sections" >> $(CIRCLE_STDLIB_CONFIG)
endif

# Enable multi-core
	@echo "DEFINE += -DARM_ALLOW_MULTI_CORE" >> $(CIRCLE_CONFIG)

# Disable delay loop calibration (boot speed improvement)
	@echo "DEFINE += -DNO_CALIBRATE_DELAY" >> $(CIRCLE_CONFIG)

# Improve I/O throughput
	@echo "DEFINE += -DNO_BUSY_WAIT" >> $(CIRCLE_CONFIG)

# Enable PWM audio output on GPIO 12/13 for the Pi Zero 2 W
	@echo "DEFINE += -DUSE_PWM_AUDIO_ON_ZERO" >> $(CIRCLE_CONFIG)

#
# Build circle-stdlib
#
circle-stdlib: $(CIRCLESTDLIBHOME)/.done

$(CIRCLESTDLIBHOME)/.done: $(CIRCLE_STDLIB_CONFIG)
	@$(MAKE) -C $(CIRCLESTDLIBHOME)
	touch $@

#
# Build mt32emu
#
mt32emu: $(MT32EMUBUILDDIR)/.done

$(MT32EMUBUILDDIR)/.done: $(CIRCLESTDLIBHOME)/.done
	@CFLAGS="$(CFLAGS_EXTERNAL)" \
	CXXFLAGS="$(CFLAGS_EXTERNAL)" \
	cmake -B $(MT32EMUBUILDDIR) \
		 $(CMAKE_TOOLCHAIN_FLAGS) \
		 -DCMAKE_CXX_FLAGS_RELEASE="-Ofast" \
		 -DCMAKE_BUILD_TYPE=Release \
		 -Dlibmt32emu_C_INTERFACE=FALSE \
		 -Dlibmt32emu_SHARED=FALSE \
		 $(MT32EMUHOME) \
		 >/dev/null
	@cmake --build $(MT32EMUBUILDDIR)
	@touch $@

#
# Build FluidSynth
#
fluidsynth: $(FLUIDSYNTHBUILDDIR)/.done

$(FLUIDSYNTHBUILDDIR)/.done: $(CIRCLESTDLIBHOME)/.done
	@${APPLY_PATCH} $(FLUIDSYNTHHOME) patches/fluidsynth-2.5.4-circle.patch

	@CFLAGS="$(CFLAGS_EXTERNAL)" \
	cmake -B $(FLUIDSYNTHBUILDDIR) \
		 $(CMAKE_TOOLCHAIN_FLAGS) \
		 -DCMAKE_C_FLAGS_RELEASE="-Ofast -fopenmp-simd" \
		 -DCMAKE_CXX_FLAGS_RELEASE="-Ofast" \
		 -DCMAKE_BUILD_TYPE=Release \
		 -DBUILD_SHARED_LIBS=OFF \
		 -Dosal=embedded \
		 -Denable-aufile=OFF \
		 -Denable-dbus=OFF \
		 -Denable-dsound=OFF \
		 -Denable-floats=ON \
		 -Denable-ipv6=OFF \
		 -Denable-jack=OFF \
		 -Denable-ladspa=OFF \
		 -Denable-libinstpatch=OFF \
		 -Denable-native-dls=OFF \
		 -Denable-libsndfile=OFF \
		 -Denable-midishare=OFF \
		 -Denable-network=OFF \
		 -Denable-oboe=OFF \
		 -Denable-openmp=OFF \
		 -Denable-opensles=OFF \
		 -Denable-oss=OFF \
		 -Denable-pipewire=OFF \
		 -Denable-pulseaudio=OFF \
		 -Denable-readline=OFF \
		 -Denable-sdl3=OFF \
		 -Denable-threads=OFF \
		 -Denable-waveout=OFF \
		 -Denable-winmidi=OFF \
		 $(FLUIDSYNTHHOME) \
		 >/dev/null
	@cmake --build $(FLUIDSYNTHBUILDDIR) --target libfluidsynth
	@touch $@

#
# Build kernel itself
#
switch-board:
	@if [ "$(SC55_TARGET)" = "unsupported" ]; then \
		echo "Board switching supports BOARD=pi3-64 or BOARD=pi4-64"; \
		exit 1; \
	fi
	@echo "Switching Circle configuration to BOARD=$(BOARD)"
	@$(RM) "$(CIRCLESTDLIBHOME)/.done"
	@$(RM) "$(CIRCLE_STDLIB_CONFIG)"
	@$(RM) "$(CIRCLE_CONFIG)"
	@find "$(CIRCLEHOME)" -type f \( -name '*.o' -o -name '*.a' \) -delete
	@$(RM) -r "$(CIRCLESTDLIBHOME)/build/circle-newlib"
	@$(RM) -r "$(CIRCLESTDLIBHOME)/install"
	@git -C "$(CIRCLESTDLIBHOME)" restore -- \
		build/.gitignore \
		build/circle-newlib/.gitignore \
		install/.gitignore
	@$(MAKE) "$(CIRCLE_STDLIB_CONFIG)" BOARD="$(BOARD)"
	@$(MAKE) circle-stdlib BOARD="$(BOARD)"
	@echo "Circle successfully configured for BOARD=$(BOARD)"

check-circle-board:
	@if [ ! -f "$(CIRCLE_STDLIB_CONFIG)" ] || [ ! -f "$(CIRCLE_CONFIG)" ]; then \
		echo "Circle is not configured for BOARD=$(BOARD)."; \
		exit 1; \
	fi
	@configured_rasppi="$$(sed -n 's/^RASPPI[[:space:]]*=[[:space:]]*//p' "$(CIRCLE_CONFIG)" | tail -1)"; \
	configured_aarch="$$(sed -n 's/^AARCH[[:space:]]*=[[:space:]]*//p' "$(CIRCLE_CONFIG)" | tail -1)"; \
	if [ "$$configured_rasppi" != "$(RASPBERRYPI)" ] || [ "$$configured_aarch" != "$(BITS)" ]; then \
		echo "Circle configuration mismatch."; \
		echo "Requested: Raspberry Pi $(RASPBERRYPI), $(BITS) bit"; \
		echo "Configured: Raspberry Pi $$configured_rasppi, $$configured_aarch bit"; \
		echo "Reconfigure Circle before switching BOARD."; \
		exit 1; \
	fi

sc55-core:
	@if [ "$(SC55_TARGET)" = "unsupported" ]; then \
		echo "SC-55 experimental build supports BOARD=pi3-64 or BOARD=pi4-64"; \
		exit 1; \
	fi
	@SC55_TARGET="$(SC55_TARGET)" \
	SC55BUILDDIR="$(CURDIR)/$(SC55BUILDDIR)" \
	bash "$(CURDIR)/build-nuked-sc55-core.sh"

all: check-circle-board circle-stdlib mt32emu sc55-core
	@$(MAKE) -f Kernel.mk \
		SC55LIB="$(SC55LIB)" \
		$(KERNEL).img $(KERNEL).hex

#
# Clean kernel only
#
clean:
	@$(MAKE) -f Kernel.mk clean

#
# Clean kernel and all dependencies
#
mrproper: clean
# Reverse patches
	@${REVERSE_PATCH} $(FLUIDSYNTHHOME) patches/fluidsynth-2.5.4-circle.patch

# Clean circle-stdlib
	@if [ -f $(CIRCLE_STDLIB_CONFIG) ]; then $(MAKE) -C $(CIRCLESTDLIBHOME) mrproper; fi
	@$(RM) $(CIRCLESTDLIBHOME)/.done

# Clean mt32emu
	@$(RM) -r $(MT32EMUBUILDDIR)

# Clean FluidSynth
	@$(RM) -r $(FLUIDSYNTHBUILDDIR)
