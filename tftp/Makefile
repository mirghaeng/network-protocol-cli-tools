# Variables
CC = gcc
CFLAGS = -g
RM = rm -f
SOURCES = server.c client.c
OBJECTS = $(SOURCES:.c=)
SERVER_INFO = server-info.txt

# Build all targets
all: $(OBJECTS) generate

# Rule to build individual targets from source files
$(OBJECTS): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Generate target for large files \
another option:	\
	@if [ -f example_file.txt ]; then \
		echo "File example_file.txt already exists. Skipping generation."; \
	else \
		dd if=/dev/urandom of=example_file.txt bs=1M count=100 && echo "File generated."; \
	fi
generate:
	@echo "Generating a large file (example_file.txt)..."
	dd if=/dev/urandom of=example_file.txt bs=1M count=100 && echo "File generated.";

# Compare original file with downloaded file
check:
	@if [ -f example_file.txt ] && [ -f output.dat ]; then \
		diff example_file.txt output.dat && echo "Files are identical." || echo "Files differ."; \
	else \
		echo "One or both files are missing. Generate and download the files first."; \
	fi

# Clean up build artifacts
clean:
	$(RM) $(OBJECTS) example_file.txt output.dat

# Kill server ports (another option: `PID=$$(lsof -t -i:$$port); sudo kill -9 $$PID` or `fuser -k $$port/tcp`)
kill:
	@if [ -f $(SERVER_INFO) ]; then \
		while read -r ip port; do \
			echo "Killing processes on port $$port..."; \
			fuser -k $$port/tcp || echo "Port $$port is not in use."; \
		done < $(SERVER_INFO); \
	fi

.PHONY: generate all check clean kill
