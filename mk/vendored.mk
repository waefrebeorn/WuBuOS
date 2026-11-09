# mk/vendored.mk -- the WuBu app suites that live in their own repositories.
#
# WuBuPad (a from-scratch C11 code editor, Notepad++ parity) and WuBuOffice
# (a C11 OOXML document suite, 100% LibreOffice/OnlyOffice parity) are
# submodules under src/apps/.  They build with their own CMake, so these
# targets drive that rather than duplicating their build logic here.

WUBUPAD_DIR    := $(APPS)/wubupad
WUBUOFFICE_DIR := $(APPS)/wubuoffice

WUBUPAD_BUILD    := $(WUBUPAD_DIR)/build
WUBUOFFICE_BUILD := $(WUBUOFFICE_DIR)/build

.PHONY: wubupad wubuoffice wubupad-test wubuoffice-test vendored vendored-test

vendored: wubupad wubuoffice

# WuBuOffice's shell compiles WuBuPad's editor core directly and its bridge
# links WuBuPad's static libs, so WuBuPad must be configured and built first.
wubupad:
	@mkdir -p $(WUBUPAD_BUILD)
	@cd $(WUBUPAD_BUILD) && cmake .. >/dev/null
	@$(MAKE) -C $(WUBUPAD_BUILD) -j$$(nproc)

wubuoffice: wubupad
	@mkdir -p $(WUBUOFFICE_BUILD)
	@cd $(WUBUOFFICE_BUILD) && cmake .. >/dev/null
	@$(MAKE) -C $(WUBUOFFICE_BUILD) -j$$(nproc)

wubupad-test: wubupad
	@ctest --test-dir $(WUBUPAD_BUILD) --output-on-failure

wubuoffice-test: wubuoffice
	@ctest --test-dir $(WUBUOFFICE_BUILD) --output-on-failure

vendored-test: wubupad-test wubuoffice-test
