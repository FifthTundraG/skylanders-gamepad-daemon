CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
CFLAGS += $(shell pkg-config --cflags glib-2.0 gio-2.0 libevdev)
LIBS = $(shell pkg-config --libs glib-2.0 gio-2.0 libevdev)

TARGET = skylanders-gamepad-daemon
SRCDIR = src
BUILDDIR = build
OUTFILE = $(BUILDDIR)/$(TARGET)

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin
SYSTEMDUNITDIR = $(PREFIX)/lib/systemd/system

# Find all .c files in SRCDIR
SOURCES := $(wildcard $(SRCDIR)/*.c)
# Create a .o path in BUILDDIR for each .c file
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

all: $(OUTFILE)

# Link the final executable
$(OUTFILE): $(OBJECTS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LIBS)

# Compile each .c to .o inside BUILDDIR
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

install: $(OUTFILE)
	install -Dm755 $(OUTFILE) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 systemd/skylanders-gamepad-daemon.service \
		$(DESTDIR)$(SYSTEMDUNITDIR)/skylanders-gamepad-daemon.service

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(SYSTEMDUNITDIR)/skylanders-gamepad-daemon.service

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean install uninstall
