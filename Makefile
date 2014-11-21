CFLAGS=-Werror -Wall -g3
INCLUDES=-I./include/ -I./deps/otree/
CC=gcc
NAME=waifu.xyz


all: dbctl test $(NAME)

clean:
	rm -f *.o
	rm -f dbctl
	rm -f greshunkel_test
	rm -f $(NAME)

test: greshunkel_test

greshunkel_test: greshunkel_test.o greshunkel.o stack.o
	$(CC) $(CLAGS) $(INCLUDES) -o greshunkel_test $^ -lm

btree.o: ./deps/otree/btree.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

dbctl: btree.o dbctl.o logging.o
	$(CC) $(CLAGS) $(INCLUDES) -o dbctl $^ -lm

$(NAME): btree.o grengine.o greshunkel.o utils.o logging.o server.o stack.o parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(INCLUDES) -o $(NAME) $^ -lm
