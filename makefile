CC=gcc
CFLAGS=`pkg-config --cflags libpubnub`
LIBS=`pkg-config --libs libpubnub`
DEPS=app.h
OBJS=app.o


app: $(OBJ)
	gcc -g app.c -o app -I. $(CFLAGS) $(LIBS) -lpthread 

clean:
	rm -f *.o app
