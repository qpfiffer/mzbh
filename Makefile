CFLAGS=-Werror -Wall -Wshadow -O2 -g3
INCLUDES=-I./include/
CC=gcc
NAME=waifu.xyz


all: $(NAME)

clean:
	rm *.o
	rm $(NAME)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

$(NAME): stack.o parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(INCLUDES) -o $(NAME) $^
