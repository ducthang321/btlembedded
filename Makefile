# Makefile cho chương trình keypad 7x4

# Compiler và flags
CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -lwiringPi -lpthread

# Tên file thực thi
TARGET = calculator

# File nguồn
SOURCES = main.c postfix.c findroot.c
OBJECTS = $(SOURCES:.c=.o)

# Thư mục
INCLUDE_DIR = include
SRC_DIR = src

# Quy tắc mặc định
all: $(TARGET)

# Liên kết các object files để tạo file thực thi
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Biên dịch các file .c thành .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Quy tắc làm sạch
clean:
	rm -f $(OBJECTS) $(TARGET)

# Quy tắc giả
.PHONY: all clean
