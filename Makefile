#
# Makefile
#
# Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
# Copyright (C) 2019-2024 Udo Steinberg, BedRock Systems, Inc.
#
# This file is part of the NOVA microhypervisor.
#
# NOVA is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# NOVA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License version 2 for more details.
#

-include Makefile.conf

# Defaults
ARCH	?= x86_64
BOARD	?= acpi
COMP	?= gcc

# Tools
INSTALL	:= install
MKDIR	:= mkdir
ifeq ($(COMP),gcc)
HST_CC	?= g++
TGT_CC	:= $(PREFIX_$(ARCH))gcc
TGT_LD	:= $(PREFIX_$(ARCH))ld
TGT_OC	:= $(PREFIX_$(ARCH))objcopy
TGT_SZ	:= $(PREFIX_$(ARCH))size
else
$(error $(COMP) is not a valid compiler type)
endif
H2E	:= $(H2E_$(ARCH))
H2B	:= $(H2B_$(ARCH))
RUN	:= $(RUN_$(ARCH))

# In-place editing works differently between GNU/BSD sed
SEDI	:= $(shell if sed --version 2>/dev/null | grep -q GNU; then echo "sed -i"; else echo "sed -i ''"; fi)

# Directories
CMD_DIR	:= cmd
SRC_DIR	:= src
INC_DIR	:= include

# Patterns
PAT_CMD	:= $(BLD_DIR)/%
PAT_OBJ	:= $(BLD_DIR)/$(ARCH)-%.o

# Files
MFL	:= $(MAKEFILE_LIST)
SRC	:= hypervisor.ld $(sort $(wildcard $(SRC_DIR)/*.S)) $(sort $(wildcard $(SRC_DIR)/*.cpp))
OBJ	:= $(patsubst %.ld,$(PAT_OBJ), $(patsubst %.S,$(PAT_OBJ), $(patsubst %.cpp,$(PAT_OBJ), $(notdir $(SRC)))))
OBJ_DEP	:= $(OBJ:%.o=%.d)

HYP	:= $(BLD_DIR)/$(ARCH)-nova
ELF	:= $(HYP).elf
BIN	:= $(HYP).bin

# Messages
ifneq ($(findstring s,$(MAKEFLAGS)),)
message = @echo $(1) $(2)
endif

# Tool check
tools = $(if $(shell command -v $($(1)) 2>/dev/null),, $(error Missing $(1)=$($(1)) *** Configure it in Makefile.conf (see Makefile.conf.example)))

# Feature check
check = $(shell if $(TGT_CC) $(1) -Werror -c -xc++ /dev/null -o /dev/null >/dev/null 2>&1; then echo "$(1)"; fi)

# Version check
gitrv = $(shell (git rev-parse HEAD 2>/dev/null || echo 0) | cut -c1-7)

# Search path
VPATH	:= $(SRC_DIR)

# Optimization options
DFLAGS	:= -MP -MMD -pipe
OFLAGS	:= -Os
ifeq ($(ARCH),x86_64)
AFLAGS	:= -Wa,--divide,--noexecstack -m64 -march=x86-64-v2 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse
else
$(error $(ARCH) is not a valid architecture)
endif

# Preprocessor options
PFLAGS	:= $(addprefix -D, $(DEFINES)) $(addprefix -I, $(INC_DIR))

# Language options
FFLAGS	:= $(or $(call check,-std=gnu++26), $(call check,-std=gnu++23), $(call check,-std=gnu++20), $(call check,-std=gnu++17))
FFLAGS	+= -ffreestanding -fdata-sections -ffunction-sections -fomit-frame-pointer
FFLAGS	+= -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -fno-use-cxa-atexit
FFLAGS	+= $(call check,-fdiagnostics-color=auto)
FFLAGS	+= $(call check,-fno-pic)
FFLAGS	+= $(call check,-fno-stack-protector)
FFLAGS	+= $(call check,-freg-struct-return)
FFLAGS	+= $(call check,-freorder-blocks)

# Warning options
WFLAGS	:= -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wdisabled-optimization -Wformat=2 -Wmissing-format-attribute -Wmissing-noreturn -Wpacked -Wpointer-arith -Wredundant-decls -Wshadow -Wwrite-strings
WFLAGS	+= -Wctor-dtor-privacy -Wno-non-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wsign-promo
WFLAGS	+= $(call check,-Wbidi-chars=any)
WFLAGS	+= $(call check,-Wlogical-op)
WFLAGS	+= $(call check,-Wstrict-null-sentinel)
WFLAGS	+= $(call check,-Wstrict-overflow=5)
WFLAGS	+= $(call check,-Wsuggest-override)
WFLAGS	+= $(call check,-Wvolatile-register-var)
WFLAGS	+= $(call check,-Wzero-as-null-pointer-constant)

ifeq ($(ARCH),aarch64)
WFLAGS	+= $(call check,-Wpedantic)
endif

# Compiler flags
CFLAGS	:= $(PFLAGS) $(DFLAGS) $(AFLAGS) $(FFLAGS) $(OFLAGS) $(WFLAGS)

# Linker flags
LFLAGS	:= --defsym=GIT_VER=0x$(call gitrv) --gc-sections --warn-common -static -n -s -T

# Rules
$(HYP):			$(OBJ)
			$(call message,LNK,$@)
			$(TGT_LD) $(LFLAGS) $^ -o $@

$(ELF):			$(HYP)
			$(call message,ELF,$@)
			$(H2E) $< $@

$(BIN):			$(HYP)
			$(call message,BIN,$@)
			$(H2B) $< $@

$(PAT_OBJ):		%.ld
			$(call message,PRE,$@)
			$(TGT_CC) $(CFLAGS) -xassembler-with-cpp -E -P -MT $@ $< -o $@
			$(SEDI) 's|$<|$(notdir $<)|' $(@:%.o=%.d)

$(PAT_OBJ):		%.S
			$(call message,ASM,$@)
			$(TGT_CC) $(CFLAGS) -c $< -o $@
			$(SEDI) 's|$<|$(notdir $<)|' $(@:%.o=%.d)

$(PAT_OBJ):		%.cpp
			$(call message,CXX,$@)
			$(TGT_CC) $(CFLAGS) -c $< -o $@
			$(SEDI) 's|$<|$(notdir $<)|' $(@:%.o=%.d)

$(PAT_CMD):		$(CMD_DIR)/%.cpp
			$(call message,CMD,$@)
			$(HST_CC) $< -o $@

$(BLD_DIR):
			$(call message,DIR,$@)
			@$(MKDIR) -p $@

Makefile.conf:
			$(call message,CFG,$@)
			@cp $@.example $@

$(OBJ):			$(MFL) | $(BLD_DIR) tool_tgt_cc

# Zap old-fashioned suffixes
.SUFFIXES:

.PHONY:			clean install run tool_hst_cc tool_tgt_cc

clean:
			$(call message,CLN,$@)
			$(RM) $(OBJ) $(HYP) $(ELF) $(BIN) $(OBJ_DEP)

install:		$(HYP)
			$(call message,INS,$^ =\> $(INS_DIR))
			$(INSTALL) -m 644 $^ $(INS_DIR)
			@$(TGT_SZ) $<

run:			$(ELF)
			$(RUN) $<

tool_hst_cc:
			$(call tools,HST_CC)

tool_tgt_cc:
			$(call tools,TGT_CC)

# Include Dependencies
ifneq ($(MAKECMDGOALS),clean)
-include		$(OBJ_DEP)
endif
