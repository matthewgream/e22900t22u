
CC = gcc
CFLAGS = -O6 -Wall -Wextra -Wpedantic
# LDFLAGS = -lmosquitto
TARGET = e22900t22u
SOURCES=include/serial_linux.h include/config_linux.h include/mqtt_linux.h include/e22xxxtxx.h
HOSTNAME = $(shell hostname)

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
install_systemd_service: $(TARGET).service
	$(call install_systemd_service,$(TARGET),$(TARGET))
install_udev_rules: 90-$(TARGET).rules
	cp 90-$(TARGET).rules $(UDEVRULES_DIR)
	udevadm control --reload-rules
	udevadm trigger
install: install_udev_rules install_systemd_service
.PHONY: install install_systemd_service install_udev_rules

