CC      = cc
CFLAGS  = -std=c11 -O3 -Wall -Wextra -Wpedantic -flto -Isrc
LDFLAGS = -flto

SRCS    = src/main.c src/generate.c
OBJS    = $(SRCS:.c=.o)
TARGET  = ranfile

PREFIX  ?= /usr/local
BINDIR  = $(PREFIX)/bin

.PHONY: all debug clean install

all: $(TARGET)

debug: CFLAGS = -std=c11 -g -O0 -Wall -Wextra -Wpedantic -fsanitize=address -Isrc
debug: LDFLAGS = -fsanitize=address
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
