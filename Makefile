all: make-libs build/json_compressor.a

make-libs:
	cd lib/fast-json-parser && make

build/json_compressor.a: build/Makefile
	cd build && make

build/Makefile: CMakeLists.txt
	rm -rf build && mkdir build && cd build && cmake ..

clean:
	rm -rf build

test: build/json_compressor.a build/test
	cd build && ./test

.PHONY: all clean test


