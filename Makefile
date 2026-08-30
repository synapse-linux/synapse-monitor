# SPDX-License-Identifier: MIT
CC ?= cc
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
BUILD_DIR ?= build
VERSION := 0.3.0-alpha.3

BASE_CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_FORTIFY_SOURCE=3 \
	-DSYNAPSE_MONITOR_VERSION='"$(VERSION)"'
BASE_CFLAGS = -O2 -g -std=c17 -Wall -Wextra -Wpedantic -Werror \
	-fstack-protector-strong -fPIE -march=x86-64 -mtune=generic
BASE_LDFLAGS = -Wl,-z,relro,-z,now -pie
REPRO_FLAGS = -ffile-prefix-map=$(abspath $(BUILD_DIR))=build \
	-fdebug-prefix-map=$(abspath $(BUILD_DIR))=build \
	-fmacro-prefix-map=$(abspath $(BUILD_DIR))=build \
	-ffile-prefix-map=$(CURDIR)=. -fdebug-prefix-map=$(CURDIR)=. \
	-fmacro-prefix-map=$(CURDIR)=.

CPPFLAGS ?=
CFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
SOURCES = src/main.c src/cli.c src/probe.c src/report.c src/inventory.c \
	src/render.c src/views.c src/tui.c
OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET = $(BUILD_DIR)/synapse-monitor

.PHONY: all clean test install
all: $(TARGET)

$(BUILD_DIR):
	install -d -m 0755 "$@"

$(BUILD_DIR)/%.o: src/%.c src/monitor.h | $(BUILD_DIR)
	$(CC) $(BASE_CPPFLAGS) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) \
		$(REPRO_FLAGS) -c -o "$@" "$<"

$(TARGET): $(OBJECTS)
	$(CC) -o "$@" $^ $(BASE_LDFLAGS) $(LDFLAGS) $(LDLIBS)

test: $(TARGET)
	./tests/run.sh "$(abspath $(TARGET))"

install: $(TARGET)
	install -D -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/synapse-monitor"
	install -D -m 0644 README.md "$(DESTDIR)$(DATADIR)/doc/synapse-monitor/README.md"
	install -D -m 0644 CHANGELOG.md "$(DESTDIR)$(DATADIR)/doc/synapse-monitor/CHANGELOG.md"
	install -D -m 0644 docs/architecture.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/architecture.md"
	install -D -m 0644 docs/json-contracts.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/json-contracts.md"
	install -D -m 0644 docs/requirements.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/requirements.md"
	install -D -m 0644 docs/view-coverage.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/view-coverage.md"
	install -D -m 0644 LICENSE "$(DESTDIR)$(DATADIR)/licenses/synapse-monitor/LICENSE"

clean:
	rm -rf "$(BUILD_DIR)"
