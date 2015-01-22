all: rbtree.c
	gcc --std=gnu99 -Wall -Werror -c -fpic rbtree.c
	gcc -shared -o librbtree.so rbtree.o

clean:
	rm ./rbtree.o ./librbtree.so
