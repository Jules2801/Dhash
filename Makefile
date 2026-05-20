CC = gcc
CFLAGS = -Wall -Wextra 
LDFLAGS = -lcrypto -pthread

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

TARGET = hachement

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(TARGET)

re: fclean all

.PHONY: all clean fclean re
