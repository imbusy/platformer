# Makefile for WebGPU + WASM Platformer

CC = emcc
CFLAGS = -O2 --use-port=emdawnwebgpu -sWASM=1 -sALLOW_MEMORY_GROWTH=1 \
	-I./shared \
	-sEXPORTED_FUNCTIONS='["_main","_malloc","_free","_upload_font_texture","_load_font_data","_network_init","_network_connect","_network_disconnect","_network_authenticate","_network_send_input","_network_send_chat","_network_on_open","_network_on_close","_network_on_error","_network_on_message"]' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","setValue","writeArrayToMemory","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
	--preload-file data/shaders@data/shaders \
	--preload-file data/fonts/mikado-medium-f00f2383.fnt@data/fonts/mikado-medium-f00f2383.fnt

SRC = src/main.c src/text.c src/math.c src/game.c src/network.c
OUT = build/game.js

.PHONY: all clean serve server

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

# Build the server (requires libwebsockets and cjson)
server:
	$(MAKE) -C server

server-clean:
	$(MAKE) -C server clean

server-run:
	$(MAKE) -C server run
