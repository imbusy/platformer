# Makefile for WebSocket Game Server

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../shared
LDFLAGS = -lwebsockets -lcjson -lm -lpthread

SRC = server.c protocol.c players.c game_sim.c chat.c
OBJ = $(SRC:.c=.o)
OUT = game_server

.PHONY: all clean run

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
server.o: server.c protocol.h players.h game_sim.h chat.h ../shared/protocol_types.h
protocol.o: protocol.c protocol.h ../shared/protocol_types.h
players.o: players.c players.h ../shared/protocol_types.h
game_sim.o: game_sim.c game_sim.h players.h ../shared/protocol_types.h
chat.o: chat.c chat.h ../shared/protocol_types.h

clean:
	rm -f $(OBJ) $(OUT)

run: $(OUT)
	./$(OUT)

debug: CFLAGS += -g -DDEBUG
debug: clean $(OUT)
