src = $(wildcard *.c)
obj = $(src:.c=.o)

CFLAGS := -Wall -O3 -fPIC
LDFLAGS := -s -pthread -lX11 -lm

all: $(basename $(src))

$(basename $(src)): $(obj)
	gcc -o $@ $< $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@ 

.PHONY: clean

clean:
	rm -f $(basename $(src)) $(obj)
