CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
CFLAGS += $(shell pkg-config --cflags glib-2.0 gio-2.0 libevdev)
LIBS = $(shell pkg-config --libs glib-2.0 gio-2.0 libevdev)

TARGET = skylandersd
SRCDIR = src
SOURCES = $(SRCDIR)/main.c
BUILDDIR = build

# Create build directory if it doesn't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(SOURCES) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILDDIR)

debug:
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LIBS: $(LIBS)"
	@echo "SOURCES: $(SOURCES)"

run: $(TARGET)
	./$(TARGET)

.PHONY: clean debug install uninstall run