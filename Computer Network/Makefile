CC=gcc
CFLAGS=-Wall -Werror -std=gnu99 -pthread


# Source files
SERVER_SRC=dbserver.c
CLIENT_SRC=dbclient.c

# Object files
SERVER_OBJ=$(SERVER_SRC:.c=.o)
CLIENT_OBJ=$(CLIENT_SRC:.c=.o)

# Executables
SERVER_BIN=dbserver
CLIENT_BIN=dbclient

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC)  $(SERVER_OBJ) -o $@ $(CFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC)  $(CLIENT_OBJ) -o $@ $(CFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

dbclient: $(CLIENT_BIN)

dbserver: $(SERVER_BIN)

clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_BIN) $(CLIENT_BIN)
