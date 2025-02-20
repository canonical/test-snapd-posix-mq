# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Zygmunt Krynicki

prefix ?= /usr/local
exec_prefix ?= $(prefix)

bindir ?= $(exec_prefix)/bin

CFLAGS ?= -Wall -Werror -Wextra
CFLSGS += -std=c11
CFLAGS += -fanalyzer
CFLAGS += -fbounds-check
CFLAGS += -g
CPPFLAGS ?=
LDFLAGS ?=
TARGET_ARCH ?=

ifneq ($(value CRAFT_ARCH_TRIPLET_BUILD_FOR),)
CC = $(subst i386,i686,$(value CRAFT_ARCH_TRIPLET_BUILD_FOR))-gcc
endif

mqctl: mq.o strlist.o errslot.o
	$(LINK.o) $(OUTPUT_OPTION) $^
mq.o: mq.c
	$(COMPILE.c) $(OUTPUT_OPTION) $^
strlist.o: strlist.c
	$(COMPILE.c) $(OUTPUT_OPTION) $^
errslot.o: errslot.c
	$(COMPILE.c) $(OUTPUT_OPTION) $^

.PHONY: fmt
fmt: $(wildcard *.[ch])
	clang-format -style=Microsoft -i $^

.PHONY: clean
clean:
	rm -f *.o mqctl

.PHONY: install
install: mqctl $(if $(DESTDIR),| $(DESTDIR))
	install -m 755 -d $(DESTDIR)$(bindir)
	install -m 755 --target-dir $(DESTDIR)$(bindir) $<

ifneq (,$(DESTDIR))
$(DESTDIR):
	mkdir -p $(DESTDIR)
endif
