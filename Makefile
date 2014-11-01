CFLAGS=-pthread -Wall -Werror -Wshadow -g3
INCLUDES=-I./include/
CC=clang
NAME=waifu.xyz


all: $(NAME)

clean:
	rm *.o
	rm $(NAME)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

$(NAME): parse.o http.o main.o parson.o
	$(CC) $(CLAGS) $(INCLUDES) -o $(NAME) $^
