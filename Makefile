CFLAGS=-O2 -g -Wall -Wextra -Werror

all: test test_sprintf

test: test.o minikermit.o
	$(CC) -o test $+
	
test_sprintf: test_sprintf.o minikermit.o
	$(CC) -o test_sprintf $+
	
clean:
	$(RM) -f *.o
	