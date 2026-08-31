# SPDX-License-Identifier: MIT
CC ?= cc
CXX ?= c++
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
BUILD_DIR ?= build
VERSION := 0.5.0-alpha.10

BASE_CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_FORTIFY_SOURCE=3 \
	-DSYNAPSE_MONITOR_VERSION='"$(VERSION)"'
BASE_CFLAGS = -O2 -g -std=c17 -Wall -Wextra -Wpedantic -Werror \
	-fstack-protector-strong -fPIE -march=x86-64 -mtune=generic
BASE_CXXFLAGS = -O2 -g -std=c++17 -Wall -Wextra -Wpedantic -Werror \
	-fstack-protector-strong -fPIC -march=x86-64 -mtune=generic
BASE_LDFLAGS = -Wl,-z,relro,-z,now -pie
REPRO_FLAGS = -ffile-prefix-map=$(abspath $(BUILD_DIR))=build \
	-fdebug-prefix-map=$(abspath $(BUILD_DIR))=build \
	-fmacro-prefix-map=$(abspath $(BUILD_DIR))=build \
	-ffile-prefix-map=$(CURDIR)=. -fdebug-prefix-map=$(CURDIR)=. \
	-fmacro-prefix-map=$(CURDIR)=.

CPPFLAGS ?=
CFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
PKG_CONFIG ?= pkg-config
BUILD_GUI ?= auto
QMAKE6 ?= qmake6
QT6_LIBEXECS := $(shell $(QMAKE6) -query QT_HOST_LIBEXECS 2>/dev/null)
MOC6 ?= $(QT6_LIBEXECS)/moc
RCC6 ?= $(QT6_LIBEXECS)/rcc
LRELEASE6 ?= lrelease6
GUI_PACKAGES := Qt6Core Qt6Gui Qt6Qml Qt6Quick Qt6QuickControls2
GUI_TEST_PACKAGES := Qt6Core Qt6Test
GUI_CXX_COMPAT := $(if $(findstring clang,$(shell $(CXX) --version 2>/dev/null)),-Wno-c++26-extensions,)
GUI_TEST_COMPAT := $(if $(findstring GCC,$(shell $(CXX) --version 2>/dev/null)),-Wno-sfinae-incomplete,)
GUI_DEPS_AVAILABLE := $(shell $(PKG_CONFIG) --exists $(GUI_PACKAGES) $(GUI_TEST_PACKAGES) \
	>/dev/null 2>&1 && test -x "$(MOC6)" && test -x "$(RCC6)" \
	&& command -v "$(LRELEASE6)" >/dev/null 2>&1 && echo 1 || echo 0)
ifeq ($(BUILD_GUI),1)
ifeq ($(GUI_DEPS_AVAILABLE),0)
$(error BUILD_GUI=1 requires Qt 6 Core/Gui/Qml/Quick/QuickControls2/Test and moc/rcc/lrelease)
endif
GUI_ENABLED := 1
else ifeq ($(BUILD_GUI),0)
GUI_ENABLED := 0
else
GUI_ENABLED := $(GUI_DEPS_AVAILABLE)
endif

SOURCES = src/main.c src/cli.c src/probe.c src/report.c src/inventory.c \
	src/render.c src/views.c src/presentation.c src/tui.c
OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET = $(BUILD_DIR)/synapse-monitor
GUI_BINARY = $(BUILD_DIR)/synapse-monitor-gui
GUI_SOURCES = gui/main.cpp gui/monitor_adapter.cpp gui/localization.cpp gui/typography.cpp
GUI_HEADERS = gui/monitor_adapter.h gui/localization.h gui/typography.h
GUI_MOC = $(BUILD_DIR)/moc_monitor_adapter.cpp $(BUILD_DIR)/moc_localization.cpp
GUI_QML = gui/qml/Main.qml gui/qml/MetricCard.qml gui/qml/GpuSummaryCard.qml gui/qml/Sparkline.qml \
	gui/qml/SynapseComboBox.qml gui/qml/DataTable.qml gui/qml/ProcessesView.qml gui/qml/PerformanceView.qml \
	gui/qml/InventoryView.qml gui/qml/InformationView.qml gui/qml/ProcessInspector.qml
GUI_QM = $(BUILD_DIR)/i18n/synapse-monitor_en_US.qm \
	$(BUILD_DIR)/i18n/synapse-monitor_it_IT.qm
GUI_QRC_FILE = $(BUILD_DIR)/resources.qrc
GUI_RCC = $(BUILD_DIR)/qrc_resources.cpp
GUI_TEST_MOC = $(BUILD_DIR)/test_gui_adapter.moc
GUI_TEST_BINARY = $(BUILD_DIR)/test-gui-adapter
ALL_TARGETS = $(TARGET)
ifeq ($(GUI_ENABLED),1)
ALL_TARGETS += $(GUI_BINARY)
endif

.PHONY: all cli gui clean test install
all: $(ALL_TARGETS)
cli: $(TARGET)

gui:
ifeq ($(GUI_ENABLED),1)
	$(MAKE) --no-print-directory "$(GUI_BINARY)" BUILD_GUI=1
else
	@echo "synapse-monitor: Qt 6 GUI SDK unavailable" >&2; exit 1
endif

$(BUILD_DIR):
	install -d -m 0755 "$@"

$(BUILD_DIR)/%.o: src/%.c src/monitor.h | $(BUILD_DIR)
	$(CC) $(BASE_CPPFLAGS) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) \
		$(REPRO_FLAGS) -c -o "$@" "$<"

$(TARGET): $(OBJECTS)
	$(CC) -o "$@" $^ $(BASE_LDFLAGS) $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/moc_monitor_adapter.cpp: gui/monitor_adapter.h | $(BUILD_DIR)
	"$(MOC6)" -f monitor_adapter.h -o "$@" "$<"

$(BUILD_DIR)/moc_localization.cpp: gui/localization.h | $(BUILD_DIR)
	"$(MOC6)" -f localization.h -o "$@" "$<"

$(BUILD_DIR)/i18n/%.qm: gui/i18n/%.ts | $(BUILD_DIR)
	install -d -m 0755 "$(BUILD_DIR)/i18n"
	"$(LRELEASE6)" -silent -fail-on-unfinished -fail-on-invalid "$<" -qm "$@"

$(GUI_QRC_FILE): $(GUI_QML) $(GUI_QM) | $(BUILD_DIR)
	printf '%s\n' '<!DOCTYPE RCC><RCC version="1.0"><qresource prefix="/">' \
		'<file alias="qml/Main.qml">$(abspath gui/qml/Main.qml)</file>' \
		'<file alias="qml/MetricCard.qml">$(abspath gui/qml/MetricCard.qml)</file>' \
		'<file alias="qml/GpuSummaryCard.qml">$(abspath gui/qml/GpuSummaryCard.qml)</file>' \
		'<file alias="qml/Sparkline.qml">$(abspath gui/qml/Sparkline.qml)</file>' \
		'<file alias="qml/SynapseComboBox.qml">$(abspath gui/qml/SynapseComboBox.qml)</file>' \
		'<file alias="qml/DataTable.qml">$(abspath gui/qml/DataTable.qml)</file>' \
		'<file alias="qml/ProcessesView.qml">$(abspath gui/qml/ProcessesView.qml)</file>' \
		'<file alias="qml/PerformanceView.qml">$(abspath gui/qml/PerformanceView.qml)</file>' \
		'<file alias="qml/InventoryView.qml">$(abspath gui/qml/InventoryView.qml)</file>' \
		'<file alias="qml/InformationView.qml">$(abspath gui/qml/InformationView.qml)</file>' \
		'<file alias="qml/ProcessInspector.qml">$(abspath gui/qml/ProcessInspector.qml)</file>' \
		'<file alias="i18n/synapse-monitor_en_US.qm">$(abspath $(BUILD_DIR)/i18n/synapse-monitor_en_US.qm)</file>' \
		'<file alias="i18n/synapse-monitor_it_IT.qm">$(abspath $(BUILD_DIR)/i18n/synapse-monitor_it_IT.qm)</file>' \
		'</qresource></RCC>' >"$@"

$(GUI_RCC): $(GUI_QRC_FILE) | $(BUILD_DIR)
	"$(RCC6)" -o "$@" "$<"

$(GUI_BINARY): $(GUI_SOURCES) $(GUI_HEADERS) $(GUI_MOC) $(GUI_RCC) $(TARGET) | $(BUILD_DIR)
	$(CXX) $(BASE_CPPFLAGS) $(CPPFLAGS) $(BASE_CXXFLAGS) $(CXXFLAGS) \
		$(REPRO_FLAGS) $(GUI_CXX_COMPAT) -Igui \
		$$( $(PKG_CONFIG) --cflags $(GUI_PACKAGES) ) -o "$@" \
		$(GUI_SOURCES) $(GUI_MOC) $(GUI_RCC) $(BASE_LDFLAGS) $(LDFLAGS) \
		$$( $(PKG_CONFIG) --libs $(GUI_PACKAGES) )

$(GUI_TEST_MOC): tests/test_gui_adapter.cpp | $(BUILD_DIR)
	"$(MOC6)" -o "$@" "$<"

$(GUI_TEST_BINARY): tests/test_gui_adapter.cpp gui/monitor_adapter.cpp \
		gui/monitor_adapter.h $(BUILD_DIR)/moc_monitor_adapter.cpp $(GUI_TEST_MOC) | $(BUILD_DIR)
	$(CXX) $(BASE_CPPFLAGS) $(CPPFLAGS) $(BASE_CXXFLAGS) $(CXXFLAGS) \
		$(REPRO_FLAGS) $(GUI_CXX_COMPAT) $(GUI_TEST_COMPAT) -Igui -I$(BUILD_DIR) \
		$$( $(PKG_CONFIG) --cflags $(GUI_TEST_PACKAGES) ) -o "$@" \
		tests/test_gui_adapter.cpp gui/monitor_adapter.cpp \
		$(BUILD_DIR)/moc_monitor_adapter.cpp $(BASE_LDFLAGS) $(LDFLAGS) \
		$$( $(PKG_CONFIG) --libs $(GUI_TEST_PACKAGES) )

test: $(ALL_TARGETS) $(if $(filter 1,$(GUI_ENABLED)),$(GUI_TEST_BINARY))
	./tests/run.sh "$(abspath $(TARGET))"
ifeq ($(GUI_ENABLED),1)
	./tests/gui-run.sh "$(abspath $(TARGET))" "$(abspath $(GUI_BINARY))" \
		"$(abspath $(GUI_TEST_BINARY))"
endif

install: $(ALL_TARGETS)
	install -D -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/synapse-monitor"
ifeq ($(GUI_ENABLED),1)
	install -D -m 0755 "$(GUI_BINARY)" "$(DESTDIR)$(BINDIR)/synapse-monitor-gui"
	install -D -m 0644 data/org.synapse.Monitor.desktop \
		"$(DESTDIR)$(DATADIR)/applications/org.synapse.Monitor.desktop"
endif
	install -D -m 0644 README.md "$(DESTDIR)$(DATADIR)/doc/synapse-monitor/README.md"
	install -D -m 0644 CHANGELOG.md "$(DESTDIR)$(DATADIR)/doc/synapse-monitor/CHANGELOG.md"
	install -D -m 0644 docs/architecture.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/architecture.md"
	install -D -m 0644 docs/json-contracts.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/json-contracts.md"
	install -D -m 0644 docs/gui-contracts.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/gui-contracts.md"
	install -D -m 0644 docs/requirements.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/requirements.md"
	install -D -m 0644 docs/view-coverage.md \
		"$(DESTDIR)$(DATADIR)/doc/synapse-monitor/view-coverage.md"
	install -D -m 0644 schemas/presentation-v1.schema.json \
		"$(DESTDIR)$(DATADIR)/synapse-monitor/schemas/presentation-v1.schema.json"
	install -D -m 0644 schemas/stream-frame-v1.schema.json \
		"$(DESTDIR)$(DATADIR)/synapse-monitor/schemas/stream-frame-v1.schema.json"
	install -D -m 0644 LICENSE "$(DESTDIR)$(DATADIR)/licenses/synapse-monitor/LICENSE"

clean:
	rm -rf "$(BUILD_DIR)"
