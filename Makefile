
CC=gcc
CFLAGS_DEFINES=
CFLAGS_COMMON=-Wall -Wextra -Wpedantic
CFLAGS_STRICT=-Werror \
    -Wstrict-prototypes \
    -Wold-style-definition \
    -Wcast-align -Wcast-qual -Wconversion \
    -Wfloat-equal -Wformat=2 -Wformat-security \
    -Winit-self -Wjump-misses-init \
    -Wlogical-op -Wmissing-include-dirs \
    -Wnested-externs -Wpointer-arith \
    -Wredundant-decls -Wshadow \
    -Wstrict-overflow=2 -Wswitch-default \
    -Wundef \
    -Wunreachable-code -Wunused \
    -Wwrite-strings
CFLAGS_OPT=-O6
CFLAGS_INCLUDES=
CC_MACHINE := $(shell $(CC) -dumpmachine)
ifneq ($(findstring x86_64,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else ifneq ($(findstring i686,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else ifneq ($(findstring i386,$(CC_MACHINE)),)
    CFLAGS_NO_FLOATING_POINT = -mno-sse -mno-mmx -mno-80387
else
    CFLAGS_NO_FLOATING_POINT =
endif
CFLAGS=$(CFLAGS_COMMON) $(CFLAGS_STRICT) $(CFLAGS_DEFINES) $(CFLAGS_OPT) $(CFLAGS_INCLUDES) $(CFLAGS_NO_FLOATING_POINT)
LDFLAGS=
TARGET=e22900t22u
SOURCES=include/serial_linux.h include/config_linux.h include/mqtt_linux.h include/util_linux.h include/e22xxxtxx.h
HOSTNAME=$(shell hostname)

##

all: $(TARGET) $(TARGET)tomqtt

$(TARGET): $(TARGET).c $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)
$(TARGET)tomqtt: $(TARGET)tomqtt.c $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET)tomqtt $(TARGET)tomqtt.c $(LDFLAGS) -lmosquitto
clean:
	rm -f $(TARGET) $(TARGET)tomqtt
format:
	clang-format -i *.c include/*.h esp32/src/*cpp
test: $(TARGET)
	./$(TARGET)
testmqtt: $(TARGET)tomqtt
	./$(TARGET)tomqtt --config=$(TARGET)tomqtt.cfg-$(HOSTNAME) --debug=true
.PHONY: all clean format test lint

##

SYSTEMD_DIR = /etc/systemd/system
UDEVRULES_DIR = /etc/udev/rules.d
define install_systemd_service
	-systemctl stop $(1) 2>/dev/null || true
	-systemctl disable $(1) 2>/dev/null || true
	cp $(2).service $(SYSTEMD_DIR)/$(1).service
	systemctl daemon-reload
	systemctl enable $(1)
	systemctl start $(1) || echo "Warning: Failed to start $(1)"
endef
install_systemd_service: $(TARGET)tomqtt.service
	$(call install_systemd_service,$(TARGET)tomqtt,$(TARGET)tomqtt)
install_udev_rules: 90-$(TARGET).rules
	cp 90-$(TARGET).rules $(UDEVRULES_DIR)
	udevadm control --reload-rules
	udevadm trigger
install: install_udev_rules install_systemd_service
restart:
	systemctl restart $(TARGET)tomqtt
.PHONY: install install_systemd_service install_udev_rules

