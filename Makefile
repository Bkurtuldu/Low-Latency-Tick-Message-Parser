# --- Compiler (defaults to your local GCC 8.5.0) ---
CXX ?= /usr/local/gcc-8.5.0/bin/g++-8.5.0

# --- Flags ---
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -pedantic
INCS     := -Iinclude

# --- Files ---
SRCS := src/main.cpp src/moldudp64.cpp src/itch.cpp
OBJS := $(SRCS:.cpp=.o)
BIN  := reader

# --- Build ---
all: clean $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

# --- Helpers ---
run: $(BIN)
	./$(BIN) $(ARGS)

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all run clean
