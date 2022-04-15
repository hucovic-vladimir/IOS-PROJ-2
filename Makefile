CC = gcc
CCFLAGS = -std=gnu99 -pedantic -Wall -Wextra -g #-Werror
LDFLAGS = -fsanitize=address
NAME = proj2


$(NAME): $(NAME).o
	$(CC) $^ -o $@ $(LDFLAGS)

$(NAME).o: $(NAME).c
	$(CC) $(CCFLAGS) $^ -c

run: $(NAME)
	./proj2 10 10 10 10

clean:
	rm $(NAME) *.o