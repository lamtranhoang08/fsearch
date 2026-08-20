CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
BIN := fsearch

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    WATCHER_SRC := src/file_watcher_linux.cpp
endif
ifeq ($(UNAME_S),Darwin)
    WATCHER_SRC := src/file_watcher_macos.cpp
endif

SRC := src/tokenizer.cpp src/crawler.cpp src/inverted_index.cpp src/main.cpp $(WATCHER_SRC)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN) *.idx