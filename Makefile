
all:
	gcc -std=c11 -Wall -Wextra -Wpedantic $(wildcard src/*.c)

