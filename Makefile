#------------------------------------------------------------------------------
# CPE/CSC 159 SPEDE3 Tools Makefile
# California State University, Sacramento
#------------------------------------------------------------------------------

SPEDE_ROOT ?= /opt/spede

BINDIR = $(SPEDE_ROOT)/bin
ETCDIR = $(SPEDE_ROOT)/etc

SRC_DIR=src
BUILD_DIR=build
INC=-Iinclude -Isrc

WARNINGS = -Wall
CC := gcc

CFLAGS += -Wall $(DEFINES)
LDFLAGS +=

SPEDE_FLINT = fl
SPEDE_DOWNLOAD = dl
SPEDE_FLASHSUP = flash-sup

ALL_TARGETS=$(SPEDE_FLINT) $(SPEDE_DOWNLOAD) $(SPEDE_FLASHSUP)

OBJS_FLINT = $(BUILD_DIR)/flint.o $(BUILD_DIR)/flsh.o
OBJS_DOWNLOAD = $(BUILD_DIR)/download.o
OBJS_FLASHSUP = $(BUILD_DIR)/flash-sup.o

src_to_bin_dir = $(patsubst $(SRC_DIR)%,$(BUILD_DIR)%,$1)
sources = $(wildcard src/*.c)
objects = $(call src_to_bin_dir,$(addsuffic .o,$(basename $(sources))))
depends = $(patsubst %.o,%.d,$(objects))

all: $(ALL_TARGETS)

clean:
	@rm -f $(BUILD_DIR)/*

install: $(ALL_TARGETS)
	@install -d $(SPEDE_ROOT)
	@install -d $(BINDIR)
	@install -d $(ETCDIR)
	@(cd $(BUILD_DIR); install -m 0755 $(ALL_TARGETS) $(BINDIR))

$(SPEDE_FLINT): $(OBJS_FLINT)
	@$(CC) $(LDFLAGS) -o $(BUILD_DIR)/$(SPEDE_FLINT) $(OBJS_FLINT) $(LIBS)

$(SPEDE_DOWNLOAD): $(OBJS_DOWNLOAD)
	@$(CC) $(LDFLAGS) -o $(BUILD_DIR)/$(SPEDE_DOWNLOAD) $(OBJS_DOWNLOAD) $(LIBS)

$(SPEDE_FLASHSUP): $(OBJS_FLASHSUP)
	@$(CC) $(LDFLAGS) -o $(BUILD_DIR)/$(SPEDE_FLASHSUP) $(OBJS_FLASHSUP) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INC) -c -o $@ $<

%.o: $(BUILD_DIR)/%.o
	@mkdir -p $(@D)
