all: bootest

bootest: scanner.c bootest.c namugen.c sds/sds.c list.c
	cc -Wno-extended-offsetof -DVERBOSE -pedantic -g scanner.c list.c bootest.c namugen.c sds/sds.c -o test.out

app.dylib: entry.c utils.c uwsgi.h app.c
	cc -fPIC -g -shared -undefined dynamic_lookup -o app.dylib `uwsgi --cflags` entry.c utils.c sds/sds.c app.c

run_app: app.dylib
	uwsgi --dlopen ./app.dylib --http :7770  --symcall _pine_entry_point --http-modifier1 18

debug_app: app.dylib
	lldb -- uwsgi --dlopen ./app.dylib --http :7770  --symcall _pine_entry_point --http-modifier1 18

compression_test: compression_test.c
	cc -O3 -g -o compression_test compression_test.c lz4/lib/lz4.c lz4/lib/lz4hc.c

uwsgi.h:
	uwsgi --dot-h > uwsgi.h
valgrind:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./test.out example/testwiki.namu
clean: 
	rm -f test.out
	rm -f app.dylib
	rm -rf compression_test
