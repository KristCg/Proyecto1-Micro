CC = g++
CFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
LIBS = -lncurses -lpthread
TARGET = galaga
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
