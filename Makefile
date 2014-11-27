CFLAGS=-Werror -Wall -O2 -g3
INCLUDES=-pthread -I./include/
CC=clang
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

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

ctl: dbctl
dbctl: models.o blue_midnight_wish.o parson.o utils.o dbctl.o db.o logging.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o dbctl $^ -lm $(LIBS)

bin: $(NAME)
$(NAME): blue_midnight_wish.o models.o grengine.o greshunkel.o db.o utils.o logging.o server.o stack.o parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ -lm $(LIBS)
