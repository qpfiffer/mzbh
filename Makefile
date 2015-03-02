CFLAGS=-Werror -Wno-missing-field-initializers -Wextra -Wall -O0 -g3
INCLUDES=-pthread -I./include/
LIBS=-lm -lrt -l38moths
NAME=waifu.xyz
COMMON_OBJ=benchmark.o blue_midnight_wish.o http.o models.o db.o parson.o utils.o logging.o


all: ctl bin test $(NAME)

clean:
	rm -f *.o
	rm -f dbctl
	rm -f greshunkel_test
	rm -f unit_test
	rm -f $(NAME)

test: unit_test
unit_test: $(COMMON_OBJ) server.o stack.o parse.o utests.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o unit_test $^ $(LIBS)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

ctl: dbctl
dbctl: $(COMMON_OBJ) dbctl.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o dbctl $^ $(LIBS)

bin: $(NAME)
$(NAME): $(COMMON_OBJ) server.o stack.o parse.o main.o parson.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ $(LIBS)
