# Makefile for WebGPU + WASM Platformer

CC = emcc
CFLAGS = -O2 --use-port=emdawnwebgpu -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'

SRC = src/main.c
OUT = build/game.js

.PHONY: all clean serve

all: $(OUT) build/index.html

$(OUT): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

build/index.html: src/index.html
	@mkdir -p build
	cp src/index.html build/index.html

clean:
	rm -rf build

serve:
	cd build && python3 -m http.server 8080
