CXX      ?= g++

CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -pedantic
INCS     := -Iinclude

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:.cpp=.o)
BIN  := strategy_simulator

all: clean $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

run: $(BIN)
	./$(BIN) $(ARGS)

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all run clean
