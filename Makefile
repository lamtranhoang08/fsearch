CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC := src/tokenizer.cpp src/crawler.cpp src/inverted_index.cpp src/main.cpp
BIN := fsearch

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN) *.idx
