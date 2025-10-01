.PHONY: build format

build:
	cmake -B build
	$(MAKE) -C build

format:
	astyle -n --max-code-length=90 src/*.c
