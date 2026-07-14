# G1-G3 compatibility wrapper.  Source lists, compiler flags, warning policy,
# manifests, and ELF checks have one canonical owner:
#   ../build_tools/build_arm_profile.ps1
# This wrapper intentionally exposes only NOT_FOR_FLASH builds.  Board builds
# are blocked until G4 creates a separate generated-SoC policy verifier.

APP_PROFILE ?= competition
ARM_BACKEND ?= disabled
BUILD_DIR ?= ../build/arm_profiles
POWERSHELL ?= powershell

.PHONY: all info clean board-build

all:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File ../build_tools/build_arm_profile.ps1 -Profile $(APP_PROFILE) -Backend $(ARM_BACKEND) -OutputRoot "$(BUILD_DIR)"

info:
	@echo Canonical builder: ../build_tools/build_arm_profile.ps1
	@echo APP_PROFILE=$(APP_PROFILE) ARM_BACKEND=$(ARM_BACKEND) NOT_FOR_FLASH=1

board-build:
	@echo BoardBuild is blocked until G4.
	@false

clean:
	@echo G1-G3 artifacts are deliberately kept outside the source tree by the caller.
