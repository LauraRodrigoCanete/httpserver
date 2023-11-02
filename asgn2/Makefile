EXECBIN  = httpserver
SOURCES  = $(wildcard *.c)
OBJECTS  = $(SOURCES:%.c=%.o) asgn2_helper_funcs.a
FORMATS  = $(SOURCES:%.c=%.fmt)

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wextra -Werror -pedantic

.PHONY: all clean format

all: $(EXECBIN)

$(EXECBIN): $(OBJECTS)
	$(CC) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXECBIN) $(filter-out asgn2_helper_funcs.a, $(OBJECTS))

format: $(FORMATS)

%.fmt: %.c
	$(FORMAT) -i $<
	touch $@

