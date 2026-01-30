CXX = g++
CXXFLAGS = -O3 -std=c++23 -Iinclude -Ilib/sqlite3 -pthread
LDFLAGS = -ldl

SRC = src/main.cpp \
      src/engine/solver.cpp \
      src/engine/mutations.cpp \
      src/data/db_manager.cpp \
      src/utils/globals.cpp \
      src/core/board.cpp \
      src/core/basis.cpp \
      src/core/scoring.cpp \
      lib/sqlite3/sqlite3.c

TARGET = bin/SAmultithread_4d
CHECK_SHM_TARGET = bin/check_shm
VALIDATOR_TARGET = bin/BoardValidator
PERMUTE_TARGET = bin/PermuteMyBoard
SHARED_LIB = bin/liblogic_ffi.so

all: $(TARGET) $(CHECK_SHM_TARGET) $(VALIDATOR_TARGET) $(PERMUTE_TARGET) $(SHARED_LIB)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(CHECK_SHM_TARGET): tools/check_shm.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Iinclude/data tools/check_shm.cpp -o $(CHECK_SHM_TARGET) $(LDFLAGS)

$(VALIDATOR_TARGET): tools/BoardValidator.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) tools/BoardValidator.cpp -o $(VALIDATOR_TARGET) $(LDFLAGS)

$(PERMUTE_TARGET): tools/PermuteMyBoard.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) tools/PermuteMyBoard.cpp -o $(PERMUTE_TARGET) $(LDFLAGS)

$(SHARED_LIB): src/ffi_wrapper.cpp src/core/scoring.cpp src/core/board.cpp src/core/basis.cpp src/utils/globals.cpp src/data/db_manager.cpp src/engine/mutations.cpp lib/sqlite3/sqlite3.c
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -shared -fPIC $^ -o $(SHARED_LIB) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(CHECK_SHM_TARGET) $(VALIDATOR_TARGET) $(PERMUTE_TARGET) $(SHARED_LIB)
