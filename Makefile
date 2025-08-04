CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
CFLAGS += $(shell pkg-config --cflags glib-2.0 gio-2.0 libevdev)
LIBS = $(shell pkg-config --libs glib-2.0 gio-2.0 libevdev)

TARGET = skylanders-gamepad-daemon
SRCDIR = src
SOURCES = $(SRCDIR)/main.c
BUILDDIR = build
OUTFILE = $(BUILDDIR)/$(TARGET)

all: $(OUTFILE)

# Create build directory if it doesn't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OUTFILE): $(SOURCES) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: $(OUTFILE)
	sudo cp $< /usr/local/bin/$(TARGET)
	sudo chmod +x /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean install uninstall