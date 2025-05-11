# Variables
CC = gcc
CFLAGS = -g
RM = rm -f
SOURCES = server.c client.c
OBJECTS = $(SOURCES:.c=)

# Build all targets
all: $(OBJECTS)

# Rule to build individual targets from source files
$(OBJECTS): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Clean up build artifacts
clean:
	$(RM) $(OBJECTS)

# Kill server ports (another option: `PID=$$(lsof -t -i:$$port); sudo kill -9 $$PID` or `fuser -k $$port/tcp`)
kill:
	@port=1234; \
	echo "Killing processes on port $$port..."; \
	fuser -k $$port/tcp || echo "Port $$port is not in use."; 

.PHONY: all clean kill

# ./server 1234
# ./client 128.114.104.54 1234 student1