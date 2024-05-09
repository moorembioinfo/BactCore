CXX = g++
CXXFLAGS = -std=c++11 -O3
SRC_DIR = src
BIN_DIR = bin


all: $(BIN_DIR)/BactCore_binary $(BIN_DIR)/BactCore-lm $(BIN_DIR)/BactCore-strict $(BIN_DIR)/BactCore-lm-strict $(BIN_DIR)/BactCore-snps

$(BIN_DIR)/BactCore_binary: $(SRC_DIR)/BactCore.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore.cpp -o $(BIN_DIR)/BactCore_binary

$(BIN_DIR)/BactCore-lm: $(SRC_DIR)/BactCore-lm.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-lm.cpp -o $(BIN_DIR)/BactCore-lm

$(BIN_DIR)/BactCore-strict: $(SRC_DIR)/BactCore-strict.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-strict.cpp -o $(BIN_DIR)/BactCore-strict

$(BIN_DIR)/BactCore-lm-strict: $(SRC_DIR)/BactCore-lm-strict.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-lm-strict.cpp -o $(BIN_DIR)/BactCore-lm-strict

$(BIN_DIR)/BactCore-snps: $(SRC_DIR)/BactCore-snps.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-snps.cpp -o $(BIN_DIR)/BactCore-snps

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f $(BIN_DIR)/BactCore $(BIN_DIR)/BactCore-lm $(BIN_DIR)/BactCore-strict $(BIN_DIR)/BactCore-snps $(BIN_DIR)/BactCore-lm
