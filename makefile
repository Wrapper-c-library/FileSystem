# Makefile for library-cpp-wrapper
# Provides convenience targets that work even without CMake (using g++ directly).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Icore
CMAKE    ?= cmake
BUILD_DIR ?= build

OBJ_DIR   := obj
SRC       := core/filemanager.cpp
OBJ       := $(OBJ_DIR)/filemanager.o
LIB       := liblibrary-cpp-wrapper.a
EXAMPLES  := basic_usage advanced_features
TESTS     := test_filemanager

.PHONY: all lib examples tests clean cmake-configure cmake-build cmake-install test run-tests

all: lib

# --- Direct build using g++ (no CMake required) ---------------------------
lib: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ): $(SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(OBJ)
	ar rcs $@ $^

examples: lib $(EXAMPLES)

basic_usage: examples/basic_usage.cpp lib
	$(CXX) $(CXXFLAGS) examples/basic_usage.cpp $(OBJ) -o $@

advanced_features: examples/advanced_features.cpp lib
	$(CXX) $(CXXFLAGS) examples/advanced_features.cpp $(OBJ) -o $@

tests: $(TESTS)

test_filemanager: tests/test_filemanager.cpp lib
	$(CXX) $(CXXFLAGS) tests/test_filemanager.cpp $(OBJ) -o $@

test: run-tests
run-tests: test_filemanager
	./test_filemanager

# --- CMake-based build ----------------------------------------------------
cmake-configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON

cmake-build: cmake-configure
	$(CMAKE) --build $(BUILD_DIR)

cmake-install: cmake-build
	$(CMAKE) --install $(BUILD_DIR) --prefix install

# --- Cleanup --------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR) $(LIB) $(EXAMPLES) $(TESTS) $(BUILD_DIR) install
