CC=gcc
CFLAGS=
objects=service

all: $(objects)

$(objects): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f ${objects}	
