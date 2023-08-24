VPATH = .

CC = gcc
DEBUGGER = gdb
TEST_FILE = test.lox
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2 -Wno-unused-function

BUILD := build

ifeq ($(OS),Windows_NT)
    ifneq ($(wildcard ./$(BUILD)),)
        # $(info Build directory exists.)
    else
        $(error Build directory does not exist. 请在目录`src`下创建目录`build`)
        # $(error Build directory does not exist. Please create it manually under src directory and run make again.)
    endif
else
    $(shell mkdir -p $(BUILD))
endif

SRCS := $(wildcard *.c)
DEPS := $(patsubst %.c,$(BUILD)/%.d,$(SRCS))
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

ifeq ($(OS),Windows_NT)
    EXEC = main.exe
else
    EXEC = main.out
endif

.PHONY: noop build run clean test gdb
noop:

build: $(EXEC)

# -include $(DEPS)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.d: %.c
	$(CC) $(CFLAGS) -MM $< -MT "$(BUILD)/$*.o" -MF $@ 

run: $(EXEC)
	./$(EXEC)

clean:
	rm $(BUILD)/*.d
	rm $(BUILD)/*.o
	rm "$(EXEC)"

test: $(EXEC)
	./$(EXEC) $(TEST_FILE)

gdb: $(EXEC)
	$(DEBUGGER) --args $(EXEC) $(TEST_FILE)