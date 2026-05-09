.PHONY: help build run clean format lint

.DEFAULT_GOAL := help

# Colors for output
BOLD   := \033[1m
GREEN  := \033[32m
YELLOW := \033[33m
CYAN   := \033[36m
RESET  := \033[0m

# Project variables
PROJECT_NAME := C FFT Visualizer
BUILD_DIR := build
EXECUTABLE := $(BUILD_DIR)/c_fft_visualizer

# Compiler settings
CC := clang
CFLAGS := -std=c99 -O3 -march=native -Wall -Wextra -Werror -pedantic
LDLIBS := -lm -lraylib -lfftw3 -lportaudio -lpthread
INCLUDE_DIRS := -I./include

# Source files
SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))
COMPILE_DB := $(BUILD_DIR)/compile_commands.json

##@ Help
help: ## Display this help message
	@awk 'BEGIN {FS = ":.*##"; printf "\n$(BOLD)$(PROJECT_NAME)$(RESET)\n\n"} \
	      /^[a-zA-Z_-]+:.*?##/ { printf "$(CYAN)%-20s$(RESET) %s\n", $$1, $$2 } \
	      /^##@/ { printf "\n$(BOLD)%s$(RESET)\n", substr($$0, 5) }' $(MAKEFILE_LIST)

##@ Build Targets
build: $(EXECUTABLE) $(COMPILE_DB) ## Build the project
	@printf "$(GREEN)✓ Build complete$(RESET)\n"

$(EXECUTABLE): $(OBJ_FILES)
	@printf "$(YELLOW)Linking $(EXECUTABLE)...$(RESET)\n"
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) -o $@ $^ $(LDLIBS)
	@cp -r assets $(BUILD_DIR)

$(COMPILE_DB): $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	@printf "Generating compile_commands.json...\n"
	@{ \
		echo '['; \
		first=true; \
		for f in $(SRC_FILES); do \
			obj=$(BUILD_DIR)/$$(basename $$f .c).o; \
			$$first && first=false || echo ','; \
			printf '  {\n    "directory": "%s",\n    "command": "%s %s %s -c %s -o %s",\n    "file": "%s"\n  }' \
				"$$(pwd)" "$(CC)" "$(CFLAGS)" "$(INCLUDE_DIRS)" "$$f" "$$obj" "$$f"; \
		done; \
		echo ''; \
		echo ']'; \
	} > $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	@printf "$(YELLOW)Compiling $<...$(RESET)\n"
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

##@ Running
run: build ## Run the application
	@printf "$(GREEN)Running $(PROJECT_NAME)...$(RESET)\n"
	@./$(EXECUTABLE)

##@ Cleaning
clean: ## Remove build directory
	@printf "$(YELLOW)Cleaning build...$(RESET)\n"
	@rm -rf $(BUILD_DIR)
	@printf "$(GREEN)✓ Clean complete$(RESET)\n"

##@ Code Quality
format: ## Format code using clang-format
	@printf "$(YELLOW)Formatting code...$(RESET)\n"
	@find src include \( -name "*.c" -o -name "*.h" \) -exec clang-format -i --style=file {} +
	@printf "$(GREEN)✓ Code formatted$(RESET)\n"

lint: build ## Lint code by building (clang with -Werror ensures no warnings)
	@printf "$(GREEN)✓ Code passes linting (no warnings/errors)$(RESET)\n"

##@ Project Information
info: ## Display project information
	@printf "$(BOLD)$(PROJECT_NAME) - Build Information$(RESET)\n"
	@printf "\n"
	@printf "Compiler: $(CC)\n"
	@printf "Flags: $(CFLAGS)\n"
	@printf "Libraries: $(LDLIBS)\n"
	@printf "Source files: $(SRC_FILES)\n"
	@printf "Executable: $(EXECUTABLE)\n"
	@printf "\n"
	@printf "Usage examples:\n"
	@printf "  $(CYAN)make build$(RESET)     # Build the project\n"
	@printf "  $(CYAN)make run$(RESET)       # Build and run\n"
	@printf "  $(CYAN)make clean$(RESET)     # Clean build files\n"
	@printf "  $(CYAN)make format$(RESET)    # Format code\n"
	@printf "\n"

.SILENT: info help