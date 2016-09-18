CXXFLAGS := -std=c++14 -Wall -Wextra -pedantic-errors
CXXFLAGS += $(shell pkg-config --cflags sdl2)
CXXFLAGS += -fdiagnostics-color=always
LDFLAGS := $(shell pkg-config --libs sdl2)

SOURCES := $(shell find . -name '*.cc')
OBJS := $(SOURCES:.cc=.o)
DEPS := $(SOURCES:.cc=.d)

APP := supaplex

all: debug-build

debug-build: CXXFLAGS += -g -ggdb3
debug-build: build

build: $(APP)

$(APP): $(OBJS)
	$(strip $(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@)

$(foreach file,$(DEPS),$(eval -include $(file)))

%.o: %.cc
	$(strip $(COMPILE.cpp) -MMD $< -o $@)

clean:
	$(RM) -f $(OBJS) $(DEPS) $(APP)

.PHONY: all debug-build build clean
