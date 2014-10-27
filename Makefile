CFLAGS=-pthread -Wall -Werror -Wshadow -g3
INCLUDES=-I./include/
CC=clang
NAME=waifu.xyz


all: $(NAME)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

$(NAME): main.o parson.o
	$(CC) $(CLAGS) $(INCLUDES) -o $(NAME) $^
