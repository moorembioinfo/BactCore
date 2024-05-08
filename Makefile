# Define the compiler to use
CXX = g++ 
# Define compilation flags
CXXFLAGS = -std=c++11 -O3

# Define the source directory
SRC_DIR = src
BIN_DIR = bin
# Define targets
all: BactCore BactCore-lm BactCore-strict BactCore-snps

BactCore: $(SRC_DIR)/BactCore.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore.cpp -o $(BIN_DIR)/BactCore

BactCore-lm: $(SRC_DIR)/BactCore-lm.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-lm.cpp -o $(BIN_DIR)/BactCore-lm

BactCore-strict: $(SRC_DIR)/BactCore-strict.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-strict.cpp -o $(BIN_DIR)/BactCore-strict

BactCore-snps: $(SRC_DIR)/BactCore-snps.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-snps.cpp -o $(BIN_DIR)/BactCore-snps

clean:
	rm -f BactCore BactCore-lm BactCore-strict BactCore-snps
