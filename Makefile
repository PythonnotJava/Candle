CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
RUNTIME_FLAGS = -std=c11 -O2 -fopenmp -lm
SRCDIR = src
VENDORDIR = $(SRCDIR)/vendor
BUILDDIR = build
TARGET = candlec

SRCS = $(SRCDIR)/main.c $(SRCDIR)/lexer.c $(SRCDIR)/token.c $(SRCDIR)/util.c $(SRCDIR)/ast.c $(SRCDIR)/parser.c $(SRCDIR)/sema.c $(SRCDIR)/codegen.c
OBJS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))

VENDOR_OBJS = $(BUILDDIR)/vendor/cJSON.o $(BUILDDIR)/vendor/mongoose.o $(BUILDDIR)/vendor/sqlite3.o
VENDOR_LIBS_WIN = -lws2_32
VENDOR_LIBS = $(if $(filter Windows_NT,$(OS)),$(VENDOR_LIBS_WIN),)

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR) $(BUILDDIR)/vendor

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/vendor/cJSON.o: $(VENDORDIR)/cjson/cJSON.c
	@mkdir -p $(BUILDDIR)/vendor
	$(CC) -O2 -c -o $@ $<

$(BUILDDIR)/vendor/mongoose.o: $(VENDORDIR)/mongoose/mongoose.c
	@mkdir -p $(BUILDDIR)/vendor
	$(CC) -O2 -c -o $@ $<

$(BUILDDIR)/vendor/sqlite3.o: $(VENDORDIR)/sqlite/sqlite3.c
	@mkdir -p $(BUILDDIR)/vendor
	$(CC) -O2 -DSQLITE_THREADSAFE=0 -c -o $@ $<

vendor: $(VENDOR_OBJS)

# Transpile a .candle file into C and build an executable.
# Usage: make run FILE=grammar/examples/parallel.candle
run: $(TARGET) $(VENDOR_OBJS)
	@./$(TARGET) --emit-c $(FILE) > $(BUILDDIR)/_out.c
	@$(CC) $(RUNTIME_FLAGS) -I $(SRCDIR) -o $(BUILDDIR)/_out.exe $(BUILDDIR)/_out.c $(VENDOR_OBJS) $(VENDOR_LIBS)
	@./$(BUILDDIR)/_out.exe

test: $(TARGET)
	./$(TARGET) --tokens grammar/examples/basic.candle

clean:
	rm -rf $(BUILDDIR) $(TARGET)

.PHONY: all clean test run vendor
