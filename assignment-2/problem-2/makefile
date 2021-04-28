NAME=prefork_server

CC=gcc
FLAGS=-Wall -Wextra
DEBUG_FLAGS=-g -fsanitize=address
SRC=prefork_server.c
LIBS=
OBJ=
RM =rm -rf

all: $(NAME)

$(NAME): $(SRC)
	$(CC) $(FLAGS) $(LIBS) $(SRC) -o $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

run:
	./$(NAME) 2 4 16

rerun: re
	./$(NAME) 2 4 16

debug: fclean
	$(CC) $(FLAGS) $(DEBUG_FLAGS) $(LIBS) $(SRC) -o $(NAME)
