BUILD_DIR ?= build
BUILD_DIR_DEBUG ?= build-debug
CMAKE     ?= cmake
JOBS      ?= $(shell nproc)

.PHONY: all build debug test run clean fmt tidy

all: build

build:
	@$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	@$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

debug:
	@$(CMAKE) -S . -B $(BUILD_DIR_DEBUG) -DCMAKE_BUILD_TYPE=Debug
	@$(CMAKE) --build $(BUILD_DIR_DEBUG) -j $(JOBS)

test: debug
	@ctest --test-dir $(BUILD_DIR_DEBUG) --output-on-failure

clean:
	@rm -rf $(BUILD_DIR) $(BUILD_DIR_DEBUG)

fmt:
	@find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' \) \
		-exec clang-format -i {} +

tidy: debug
	@find src -type f -name '*.cpp' -exec clang-tidy -p $(BUILD_DIR_DEBUG) {} +
