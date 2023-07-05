VPATH = src
all:
	$(MAKE) -C src
	mv ./src/main.exe ./Zlang.exe