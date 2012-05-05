.ALL: lab2

CFLAGS := -g -Wall -std=c99
LIBS := -ljpeg -lm

lab2: lab2-seq.o Makefile
	gcc ${CFLAGS} -o lab2 lab2-seq.o ${LIBS}

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

clean::
	rm -f *.o *~

clear: clean
	rm -f lab2

