all: scanner.c bootest.c
	cc -g -DVERBOSE scanner.c bootest.c -o test.out

valgrind:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./test.out testwiki.namu
clean: 
	rm -f test.out

