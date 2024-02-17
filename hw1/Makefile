CC=gcc
CFLAGS=
objects=server client

all: $(objects)

$(objects): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f ${objects}	
