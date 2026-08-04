# My Seed OS — Top-Level Makefile
# Builds: kernel, JIT, GUI, bridge, apps, tests
#
# This Makefile is organized into modular includes under mk/:
#   mk/vars.mk         — directory paths and compiler variables
#   mk/objects.mk     — object file lists for each subsystem
#   mk/targets.mk     — build targets (all, kernel, jit, gui, etc.)
#   mk/pattern-rules.mk — per-directory compilation pattern rules
#   mk/tests.mk       — tiered test targets (critical, high, medium, etc.)
#   mk/clean.mk       — clean rules
#
# Object lists and pattern rules are kept per-subsystem so a one-line edit
# to any .c file recompiles only that one .o + relinks, not the whole tree.

include mk/vars.mk
include mk/objects.mk
include mk/targets.mk
include mk/pattern-rules.mk
include mk/tests.mk
include mk/clean.mk
