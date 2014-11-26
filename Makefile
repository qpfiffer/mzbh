CFLAGS=-Werror -Wall -O2 -g3
INCLUDES=-pthread -I./include/ -I./OlegDB/include
LIB_INCLUDES=-L./OlegDB/build/lib
LIBS=-loleg
CC=gcc
NAME=waifu.xyz


all: ctl bin test $(NAME)

clean:
	rm -f *.o
	rm -f dbctl
	rm -f greshunkel_test
	rm -f $(NAME)

test: greshunkel_test
greshunkel_test: greshunkel_test.o greshunkel.o stack.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o greshunkel_test $^ -lm

oleg:
	$(MAKE) -C OlegDB liboleg

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

ctl: oleg dbctl
dbctl: blue_midnight_wish.o utils.o dbctl.o db.o logging.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o dbctl $^ -lm $(LIBS)

bin: oleg $(NAME)
$(NAME): blue_midnight_wish.o grengine.o greshunkel.o db.o utils.o logging.o server.o stack.o parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ -lm $(LIBS)
