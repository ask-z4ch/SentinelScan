CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
OBJDIR = obj
TARGET = sentinel.exe

SRCS = main.c scanner.c entropy.c report.c
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	@if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"

clean:
	-rd /S /Q $(OBJDIR) 2>nul
	-del /Q $(TARGET) 2>nul
	-del /Q report.csv 2>nul

.PHONY: all clean
