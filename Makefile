# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -g -std=c++17 -Iinclude

# Source files
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
ASSETS_DIR = assets

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# For more folders SRCS = $(wildcard src/*.cpp classes/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Libraries
SFML_LIBS = -lsfml-graphics  -lsfml-window  -lsfml-system  -lsfml-audio

# Binary output file
TARGET = $(BIN_DIR)/GameName

# Default rule
all: $(TARGET) 

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS)) -o $@ $^ $(SFML_LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS)) -c $< $@

run: $(TARGET)
	./$(TARGET)
	
debug: $(TARGET)
	gdb ./$(TARGET)

clean: $(TARGET)
	rm -rf./$(OBJ_DIR) $(BIN_DIR)
