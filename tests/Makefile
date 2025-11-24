CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I.. -g
LDFLAGS =

# Find all test source files
TEST_SOURCES = $(wildcard test_*.cpp)
TEST_TARGETS = $(TEST_SOURCES:.cpp=)

.PHONY: all clean run test

# Default target: build all tests
all: $(TEST_TARGETS)

# Pattern rule to build each test executable
test_%: test_%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Run all tests
run: all
	@echo "===================="
	@echo "Running all tests..."
	@echo "===================="
	@for test in $(TEST_TARGETS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "===================="
	@echo "All tests passed!"
	@echo "===================="

# Alias for run
test: run

# Clean up built files
clean:
	rm -f $(TEST_TARGETS)

# Help target
help:
	@echo "Available targets:"
	@echo "  all (default) - Build all test executables"
	@echo "  run/test      - Build and run all tests"
	@echo "  clean         - Remove all built test executables"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Test targets:"
	@for test in $(TEST_TARGETS); do \
		echo "  $$test"; \
	done
