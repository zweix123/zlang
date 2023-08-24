VPATH = src

all:
	$(MAKE) -C src build
ifeq ($(OS),Windows_NT)
	cmd /C move .\src\main.exe z.exe
else
	mv ./src/main.out ./z
endif
