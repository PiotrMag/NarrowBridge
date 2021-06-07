app: main.c
	gcc -pthread main.c

test1: app
	./a.out

test2: app
	./a.out 0 10

test3: app
	./a.out 1 10

test4: app
	./a.out 0 1000 -fast

test5: app
	./a.out 1 10 -debug

.PHONY: clear
clear:
	-rm -f *.o *.out