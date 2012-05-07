execs := lab2 lab2seq

ALL: $(execs)

CFLAGS := -g -Wall -std=c99
CPPFLAGS := -g -Wall -std=c++0x
LIBS := -ljpeg -lm

lab2seq: lab2-seq.o Makefile
	gcc ${CFLAGS} -o lab2seq lab2-seq.o ${LIBS}

lab2: lab2-base.o Makefile
	g++ ${CPPFLAGS} -o lab2 lab2-base.o ${LIBS}

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.cc
	g++ ${CPPFLAGS} -c -o $@ $<

clean::
	rm -f *.o $(execs)

clear: clean
	rm -f $(execs)

