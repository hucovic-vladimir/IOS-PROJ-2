CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
LDFLAGS = -pthread
NAME = proj2
ZIPFILE = xhucov00.zip

$(NAME): $(NAME).o
	$(CC) $(LDFLAGS) $^ -o $@

$(NAME).o: $(NAME).c
	$(CC) $(CFLAGS) $^ -c

clean:
	rm $(NAME) *.o

$(ZIPFILE): $(NAME).c Makefile
	zip $@ $^

.PHONY: clean $(ZIPFILE)