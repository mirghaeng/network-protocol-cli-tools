# Variables
CC = gcc
CFLAGS = -g
RM = rm -f
SOURCES = client.c
OBJECTS = $(SOURCES:.c=)
LDFLAGS = -L/usr/local/ssl/lib
LDLIBS = -lssl -lcrypto

# Build all targets
all: $(OBJECTS)

# Rule to build individual targets from source files
$(OBJECTS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

# Clean up build artifacts
clean:
	$(RM) $(OBJECTS) output.dat

.PHONY: all clean