CC  = gcc
CXX = g++
SRC = main.c serial.c ltc.c gpio.c 

CFLAGS = -Wall -O2 -c
LDFLAGS = -lm -lpthread -lrt
TARGET = chiptest
OBJ := $(SRC:.c=.o)
all:$(OBJ)
	$(CC)  $(OBJ) -o $(TARGET) $(LDFLAGS)
%.o:%.c
	$(CC) $(CFLAGS) $<
clean:
	-rm -f $(OBJ)
	-rm -f $(TARGET)

