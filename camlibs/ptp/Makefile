
OBJECTS	= dx3500 libptp.so

CFLAGS	= -g -DDEBUG -Wall
LDFLAGS = -lusb ptp.o

CC	= gcc

all: ${OBJECTS}

dx3500: main.o libptp.so
	${CC} -o dx3500 main.o ${LDFLAGS} -g

ptp.o:	ptp.c ptp.h

libptp.so: ptp.o
	${CC} -shared -o libptp.so ptp.o

clean:
	rm -f *.o *.so
