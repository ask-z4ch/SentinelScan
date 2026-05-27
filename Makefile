CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lm
SRCDIR = src
INCDIR = include
OBJDIR = obj
TARGET = sentinel.exe

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	@if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"

clean:
	-rd /S /Q $(OBJDIR) 2>nul
	-del /Q $(TARGET) 2>nul
	-del /Q report.csv 2>nul

.PHONY: all clean
