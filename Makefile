CFLAGS=-Werror -Wall -g3
INCLUDES=-I./include/
CC=gcc
NAME=waifu.xyz


all: test $(NAME)

clean:
	rm *.o
	rm $(NAME)

test: greshunkel_test

greshunkel_test: greshunkel_test.o greshunkel.o stack.o
	$(CC) $(CLAGS) $(INCLUDES) -o greshunkel_test $^ -lm

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

$(NAME): greshunkel.o utils.o logging.o server.o stack.o parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(INCLUDES) -o $(NAME) $^ -lm
