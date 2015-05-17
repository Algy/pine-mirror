all: bootest

bootest: scanner.c bootest.c namugen.c sds/sds.c list.c
	cc -pedantic -g scanner.c list.c bootest.c namugen.c sds/sds.c -o test.out

valgrind:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./test.out testwiki.namu
clean: 
	rm -f test.out

