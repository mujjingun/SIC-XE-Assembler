all:
	gcc -Wall -Wextra -o 20171634.out 20171634.c opcode.c history.c dump.c dir.c

clean:
	rm ./20171634.out
