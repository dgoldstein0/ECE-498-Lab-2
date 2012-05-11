execs := lab2 lab2seq

ALL: $(execs)

CFLAGS := -pg -g -Wall -std=c99 -O9
CPPFLAGS := -pg -g -Wall -std=c++0x -O9
LIBS := -ljpeg -lm -lpthread

lab2seq: lab2-seq.o Makefile
	gcc ${CFLAGS} -o lab2seq lab2-seq.o ${LIBS}

lab2: lab2-base.o Makefile
	g++ ${CPPFLAGS} -o lab2 lab2-base.o ${LIBS}

%.o: %.c Makefile
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.cc Makefile
	g++ ${CPPFLAGS} -c -o $@ $<

clean::
	rm -f *.o $(execs)

clear: clean
	rm -f $(execs)

