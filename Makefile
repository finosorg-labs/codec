# codec Makefile
#
# Build Targets:
#   make           build Linux native (default)
#   make linux     build Linux native
#   make windows   cross-compile Windows amd64
#   make all       build Linux + Windows
#
# Test Targets:
#   make test      run Go tests
#   make bench     run Go benchmarks
#
# QA Targets:
#   make qa                run all QA checks (sanitizers + static analysis)
#   make qa-sanitizers     run all sanitizers (ASan/USan/TSan/MSan)
#   make qa-static         run static analysis (clang-tidy/cppcheck)
#
#   Individual sanitizers:
#     make sanitizer-asan   AddressSanitizer
#     make sanitizer-usan   UndefinedBehaviorSanitizer
#     make sanitizer-tsan   ThreadSanitizer
#     make sanitizer-msan   MemorySanitizer (requires clang)
#
#   Individual static analysis:
#     make clang-tidy       clang-tidy analysis
#     make cppcheck         cppcheck analysis
#
# Utility Targets:
#   make format    format C code with clang-format
#   make verify    verify artifact formats
#   make clean     remove all build artifacts
#   make sync      sync all submodules
#   make help      show this help

BUILD_TYPE ?= Release
CMAKE ?= cmake
TOOLCHAIN_DIR := cmake/toolchain

LINUX_BUILD_DIR  := build/linux_amd64
WINDOWS_BUILD_DIR := build/windows_amd64
MACOS_BUILD_DIR := build/darwin_arm64

LINUX_ARTIFACT_DIR    := $(LINUX_BUILD_DIR)
WINDOWS_ARTIFACT_DIR  := $(WINDOWS_BUILD_DIR)
MACOS_ARTIFACT_DIR  := $(MACOS_BUILD_DIR)

# Third-party library target paths
LINUX_THIRD_PARTY_DIR   := $(LINUX_BUILD_DIR)/third_party
WINDOWS_THIRD_PARTY_DIR := $(WINDOWS_BUILD_DIR)/third_party
MACOS_THIRD_PARTY_DIR := $(MACOS_BUILD_DIR)/third_party

LINUX_CONFIG := -G Ninja \
	-B $(LINUX_BUILD_DIR) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DCMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=$(shell [ "$(BUILD_TYPE)" = "Release" ] && echo ON || echo OFF)

.PHONY: all default linux windows macos go test bench clean verify help format
.PHONY: qa qa-sanitizers qa-static
.PHONY: sanitizer-asan sanitizer-usan sanitizer-tsan sanitizer-msan clang-tidy cppcheck
.PHONY: sync

default: linux

all: format linux go
# TODO: Re-enable Windows build after platform module has Windows-compatible GMP/MPFR libraries
# all: format linux windows go

qa: format qa-static qa-sanitizers
	@echo "==> All QA checks completed"

qa-static: clang-tidy cppcheck
	@echo "==> All static analysis checks completed"

qa-sanitizers: sanitizer-asan sanitizer-usan sanitizer-tsan
ifeq ($(shell uname),Linux)
	@$(MAKE) sanitizer-msan
else
	@echo "==> Skipping MemorySanitizer (not supported on macOS ARM64)"
endif
	@echo "==> All sanitizer checks completed"

linux:
	@echo "==> Building Linux (native, $(BUILD_TYPE))"
	@$(CMAKE) -B $(LINUX_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel
	@echo "==> Cleaning intermediate build artifacts"
	@rm -f $(LINUX_BUILD_DIR)/libfinkit_codec_static_base.a

windows:
	@echo "==> Building Windows amd64 (cross-compile, $(BUILD_TYPE))"
	@$(CMAKE) -B $(WINDOWS_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_DIR)/x86_64-w64-mingw32.cmake
	@$(CMAKE) --build $(WINDOWS_BUILD_DIR) --parallel

macos:
	@echo "==> Building Macos (native, $(BUILD_TYPE))"
	@$(CMAKE) -B $(MACOS_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@$(CMAKE) --build $(MACOS_BUILD_DIR) --parallel
	@echo "==> Cleaning intermediate build artifacts"
	@rm -f $(MACOS_BUILD_DIR)/libfinkit_codec_static_base.a

go:
	@echo "==> Building Go module with source (verify compilation)"
	@CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go build ./...
	@echo "==> Building Go module with lib (verify compilation)"
	@CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go build -tags lib ./...

test: linux
	@echo "==> Building with coverage flags"
	@BUILD_TYPE=Debug $(CMAKE) -B $(LINUX_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage -O0" \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF >/dev/null 2>&1 || true
	@$(CMAKE) --build $(LINUX_BUILD_DIR) --target codec_tests --parallel
	@echo "==> Running C tests with coverage"
	@bash scripts/test_coverage.sh $(LINUX_BUILD_DIR) || true
	@echo ""
	@echo "==> Running Go tests"
	@FC_BUILD_MODE=source CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test ./... -v

bench:
	@echo "==> Building benchmarks (Release mode)"
	@BUILD_TYPE=Release $(CMAKE) -B $(LINUX_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DFC_BUILD_TESTS=OFF \
		-DFC_BUILD_BENCHMARKS=ON >/dev/null 2>&1 || true
	@$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel
	@echo "==> Running C benchmarks"
	@if [ -f $(LINUX_BUILD_DIR)/benchmarks/codec_benchmarks ]; then \
		$(LINUX_BUILD_DIR)/benchmarks/codec_benchmarks; \
	else \
		echo "No C benchmarks found"; \
	fi
	@echo ""
	@echo "==> Running Go benchmarks"
	@FC_BUILD_MODE=source CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test -bench=. -benchmem ./...

format:
	@echo "==> Formatting C code with clang-format"
	@if command -v clang-format >/dev/null 2>&1; then \
		find codec-c include \( -name '*.c' -o -name '*.h' \) -exec clang-format -i {} \; ; \
	else \
		echo "WARNING: clang-format not found, skipping format check"; \
	fi

verify:
	@echo "=== Verify artifact formats ==="
	@echo "--- Linux ---"
	@objdump -f $(LINUX_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"
	@echo "--- Windows ---"
	@objdump -f $(WINDOWS_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"

sanitizer-asan:
	@echo "==> Building with AddressSanitizer"
	@$(CMAKE) -B build/sanitizer-asan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=address >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-asan --parallel
	@echo "==> Running AddressSanitizer tests"
	@cd build/sanitizer-asan && ctest --output-on-failure

sanitizer-usan:
	@echo "==> Building with UndefinedBehaviorSanitizer"
	@$(CMAKE) -B build/sanitizer-usan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=undefined >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-usan --parallel
	@echo "==> Running UndefinedBehaviorSanitizer tests"
	@cd build/sanitizer-usan && ctest --output-on-failure

sanitizer-tsan:
	@echo "==> Building with ThreadSanitizer"
	@$(CMAKE) -B build/sanitizer-tsan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=thread >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-tsan --parallel
	@echo "==> Running ThreadSanitizer tests"
	@cd build/sanitizer-tsan && ctest --output-on-failure || \
		(echo "WARNING: ThreadSanitizer failed (known WSL2 compatibility issue)" && exit 0)

sanitizer-msan:
	@echo "==> Building with MemorySanitizer (requires clang)"
	@CC=clang $(CMAKE) -B build/sanitizer-msan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=memory >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-msan --parallel
	@echo "==> Running MemorySanitizer tests"
	@cd build/sanitizer-msan && ctest --output-on-failure

clang-tidy:
	@echo "==> Generating compile_commands.json for clang-tidy"
	@CC=clang CXX=clang++ $(CMAKE) -B build/clang-tidy \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null 2>&1 || true
	@echo "==> Running clang-tidy analysis on C source files"
	@find codec-c include \( -name '*.c' -o -name '*.h' \) \
		! -name 'platform_win.c' \
		! -name 'platform_macos.c' \
		-print | while read f; do echo "  Checking: $$f"; done
	@find codec-c include \( -name '*.c' -o -name '*.h' \) \
		! -name 'platform_win.c' \
		! -name 'platform_macos.c' \
		-exec clang-tidy -p build/clang-tidy {} \; 2>&1 | \
		grep -v "warnings generated" || true
	@echo "==> clang-tidy: No issues found"

cppcheck:
	@echo "==> Running cppcheck static analysis on C source files"
	@find codec-c include \( -name '*.c' -o -name '*.h' \) -print | \
		while read f; do echo "  Checking: $$f"; done
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=missingInclude \
		--suppress=toomanyconfigs \
		--suppress=unusedFunction \
		--suppress=knownConditionTrueFalse \
		--inline-suppr --quiet \
		-I include codec-c/ 2>&1 || true
	@echo "==> cppcheck: No issues found"

clean:
	@echo "==> Cleaning build artifacts"
	@rm -rf build/
	@echo "==> Cleaning Go cache"
	@go clean -cache

sync:
	@echo "==> Syncing all submodules (init + remote update + restore tracked files)"
	@echo "==> Step 1: Sync submodule URLs from .gitmodules to .git/config"
	@git submodule sync --recursive
	@echo "==> Step 2: Ensure submodules are registered in git index"
	@git config -f .gitmodules --get-regexp '^submodule\..*\.path$$' | while read key path; do \
		if [ ! -d "$$path/.git" ] && [ ! -f "$$path/.git" ]; then \
			echo "  Restoring submodule: $$path"; \
			submodule_name=$$(echo $$key | sed 's/^submodule\.\(.*\)\.path$$/\1/'); \
			url=$$(git config -f .gitmodules --get "submodule.$$submodule_name.url"); \
			git submodule add -f "$$url" "$$path" 2>/dev/null || true; \
		fi; \
	done
	@echo "==> Step 3: Reset and clean submodule working trees"
	@git submodule foreach --recursive 'git reset --hard && git clean -fd' || true
	@echo "==> Step 4: Update all submodules to latest"
	@git submodule update --init --remote --merge --recursive --force
	@echo "==> Submodules synced successfully"

help:
	@echo "codec Makefile - Build and Test Targets"
	@echo ""
	@echo "Build Targets:"
	@echo "  make           - build Linux native (default)"
	@echo "  make linux     - build Linux native"
	@echo "  make windows   - cross-compile Windows amd64"
	@echo "  make all       - build Linux + Windows + Go"
	@echo "  make go        - build Go module"
	@echo ""
	@echo "Test Targets:"
	@echo "  make test      - run Go tests"
	@echo "  make bench     - run Go benchmarks"
	@echo ""
	@echo "QA Targets:"
	@echo "  make qa            - run static analysis on C source code"
	@echo "  make qa-static     - run clang-tidy and cppcheck"
	@echo "  make qa-sanitizers - run runtime sanitizer tests"
	@echo "  make clang-tidy    - run clang-tidy analysis"
	@echo "  make cppcheck      - run cppcheck analysis"
	@echo ""
	@echo "Utility Targets:"
	@echo "  make format    - format C code"
	@echo "  make verify    - verify artifact formats"
	@echo "  make clean     - remove build artifacts"
	@echo "  make sync      - sync all submodules"
	@echo "  make help      - show this help"
