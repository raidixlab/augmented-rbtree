all: rbtree.c
	gcc --std=gnu99 -Wall -Werror -c -fpic rbtree.c
	gcc -shared -o librbtree.so rbtree.o

test: test.c rbtree.c
	gcc --std=gnu99 -O3 -Wall -Werror test.c rbtree.c -o test

clean:
	rm ./rbtree.o ./librbtree.so ./test
