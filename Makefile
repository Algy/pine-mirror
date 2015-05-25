all: bootest 
inlinelexer.yy.c: inlinelexer.l
	lex --align --outfile=inlinelexer.yy.c inlinelexer.l 

bootest: scanner.c bootest.c namugen.c sds/sds.c list.c inlinelexer.yy.c
	cc -O3 -Wno-extended-offsetof -DVERBOSE -pedantic -g scanner.c inlinelexer.yy.c list.c bootest.c namugen.c sds/sds.c -o test.out

app.dylib: entry.c utils.c uwsgi.h app.c sds/sds.c app.c data.c scanner.c inlinelexer.yy.c namugen.c list.c lz4/lib/lz4.c lz4/lib/lz4hc.c
	cc -O3 -fPIC -g -shared -undefined dynamic_lookup -I mariadb-connector-c/include -I sds/ -I hiredis/ -I hiredis/ -L hiredis/ -L mariadb-connector-c/libmariadb -lmariadb -lhiredis -o app.dylib `uwsgi --cflags` -Wno-error entry.c utils.c sds/sds.c app.c data.c scanner.c inlinelexer.yy.c namugen.c list.c lz4/lib/lz4.c lz4/lib/lz4hc.c

run_app: app.dylib
	uwsgi --async 10 --dlopen ./app.dylib --http :7770 --symcall _pine_entry_point --symcall-post-fork _pine_after_fork --http-modifier1 18

debug_app: app.dylib
	lldb -- uwsgi --dlopen ./app.dylib --http :7770  --symcall _pine_entry_point --http-modifier1 18

compression_test: compression_test.c
	cc -O3 -g -o compression_test compression_test.c lz4/lib/lz4.c lz4/lib/lz4hc.c

data_test: data.c data_test.c
	cc -g -Wall -I mariadb-connector-c/include -I sds/ -I hiredis/ -I hiredis/ -L hiredis/ -L mariadb-connector-c/libmariadb -lmariadb -lhiredis -o data_test data.c data_test.c lz4/lib/lz4.c lz4/lib/lz4hc.c sds/sds.c

mariadb_test: mariadb_test.c
	cc -g  -I mariadb-connector-c/include -I sds/ -L mariadb-connector-c/libmariadb -L hiredis/ -l mariadb mariadb_test.c sds/sds.c -o mariadb_test

uwsgi.h:
	uwsgi --dot-h > uwsgi.h
valgrind:
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./test.out example/testwiki.namu
clean: 
	rm -f inlinelexer.yy.c
	rm -f test.out
	rm -f app.dylib
	rm -rf compression_test
	rm -f mariadb_test
	rm -f data_test
