CC=gcc
AR=ar

INCLUDE=-I/opt/dev/dnd/basilisk/include \
        -I/opt/dev/dnd/basilisk \
        -I/opt/dev/utils \
				-I/usr/lib/qDecoder/src

LIBS=-lm \
     -L/opt/dev/dnd/basilisk -lbasilisk \
     /usr/lib/qDecoder/libqDecoder.a

OPTS=$(INCLUDE) -g -Wall -DUSE_COUNTER -DCTRLOCATION="\"/home/generators/treasure2.cnt\""

.SUFFIXES:

.SUFFIXES: .c .o

.c.o:
	$(CC) $(OPTS) -c -o $@ $<

all: treasure2.cgi

OBJS=\
	treasure2-cgi.c \
	/opt/dev/dnd/basilisk/bskcallbacks.c \
	/opt/dev/utils/writetem.c \
	/opt/dev/utils/wtstream.c

treasure2.cgi: $(OBJS)
	$(CC) $(OPTS) -o treasure2.cgi $(OBJS) $(LIBS)

clean:
	rm -f *.cgi
	rm -f *.o
