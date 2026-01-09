# Makefile for WebGPU + WASM Platformer

CC = emcc
CFLAGS = -O2 --use-port=emdawnwebgpu -sWASM=1 -sALLOW_MEMORY_GROWTH=1 \
	-sEXPORTED_FUNCTIONS='["_main","_malloc","_free","_on_key_down","_on_key_up","_upload_font_texture","_load_font_data"]' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","setValue","writeArrayToMemory"]' \
	--preload-file data/shaders@data/shaders \
	--preload-file data/fonts/mikado-medium-f00f2383.fnt@data/fonts/mikado-medium-f00f2383.fnt

SRC = src/main.c src/text.c src/math.c src/game.c
OUT = build/game.js

.PHONY: all clean serve

all: $(OUT) build/index.html build/data

$(OUT): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

build/index.html: src/index.html
	@mkdir -p build
	cp src/index.html build/index.html

build/data: data
	@mkdir -p build/data
	cp -r data/* build/data/

clean:
	rm -rf build

serve:
	cd build && python3 -m http.server 8080
