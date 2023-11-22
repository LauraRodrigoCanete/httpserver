SOURCES  = queue.c rwlock.c
OBJECTS  = $(SOURCES:%.c=%.o)
FORMATS  = $(SOURCES:%.c=%.fmt)

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wextra -Werror -pedantic

.PHONY: all clean format

all: $(OBJECTS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS)

format: $(FORMATS)

%.fmt: %.c
	$(FORMAT) -i $<
	touch $@
