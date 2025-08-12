CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -I. -O3 -march=native -mtune=native -flto -ffast-math -funroll-loops -DNDEBUG
LDFLAGS = -flto -O3
LIBS = -lnotcurses-core -lnotcurses -lavformat -lavcodec -lavutil -lswscale -lswresample -lao -lpthread

TARGET = ../build/video_player
SRCDIR = src
OBJDIR = ../build
SOURCES = $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/ds/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS) | build
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p ../build

clean:
	rm -rf $(OBJDIR)/*.o $(TARGET)

install-deps:
	@echo "Installing dependencies..."
	@echo "On Ubuntu/Debian:"
	@echo "  sudo apt-get install libnotcurses-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libao-dev"
	@echo "On Fedora/RHEL:"
	@echo "  sudo dnf install notcurses-devel ffmpeg-devel libao-devel"
	@echo "On Arch Linux:"
	@echo "  sudo pacman -S notcurses ffmpeg libao"

run: $(TARGET)
	./$(TARGET)

# Performance build with maximum optimizations
performance: CFLAGS += -DNDEBUG -fomit-frame-pointer -fno-stack-protector
performance: $(TARGET)

test-audio: $(AUDIO_TEST)
	./$(AUDIO_TEST)

.PHONY: all clean install-deps run test-audio performance debug
