VPATH = .

CC = gcc
DEBUGGER = gdb
TEST_FILE = test.lox
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2 -Wno-unused-function

BUILD := build
$(shell mkdir -p $(BUILD))

SRCS := $(wildcard *.c)
DEPS := $(patsubst %.c,$(BUILD)/%.d,$(SRCS))
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
EXEC = main.exe  # .exe是为兼容win

.PHONY: all clean run test gdb

all: $(EXEC)

-include $(DEPS)

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