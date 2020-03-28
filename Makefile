CFLAGS=-Werror -Wno-format-truncation -Wno-missing-field-initializers -Wextra -Wall -O0 -g3
INCLUDES=-pthread -I./include/ `pkg-config --cflags libpq`
LIBS=-l38moths -lcurl -lm -lrt `pkg-config --libs libpq`
NAME=mzbh
COMMON_OBJ=benchmark.o blue_midnight_wish.o http.o models.o db.o parson.o utils.o


all: bin downloader test $(NAME)

clean:
	rm -f *.o
	rm -f downloader
	rm -f unit_test
	rm -f $(NAME)

test: unit_test
unit_test: $(COMMON_OBJ) server.o stack.o parse.o utests.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o unit_test $^ $(LIBS)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

bin: $(NAME)
$(NAME): $(COMMON_OBJ) server.o main.o parson.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ $(LIBS)

downloader: $(COMMON_OBJ) parse.o stack.o downloader.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o downloader $^ $(LIBS)
