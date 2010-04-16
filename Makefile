CC=gcc-4.5
CFLAGS+= -I/usr/lib/gcc/i486-linux-gnu/4.5/plugin/include -fPIC

dumb_plugin.so: dumb_plugin.o
	$(CC) -shared $^ -o $@
	g++-4.5 -O -fplugin=./dumb_plugin.so -fplugin-arg-dumb_plugin-ref-pass-name=ccp -fplugin-arg-dumb_plugin-ref-pass-instance-num=1 -c dumb-plugin-test-1.C

alwayszero_plugin.so: alwayszero_plugin.o
	$(CC) -shared $^ -o $@
	gcc-4.5 -O -fplugin=./alwayszero_plugin.so alwayszero-test.c -o zero.out
	./zero.out

clean:
	rm -f *o *~ *out