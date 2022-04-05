build: so-stdio.o
	cl -shared so-stdio.o -o libso_stdio.so
so-stdio.o: so-stdio.c
	gcc -fPIC -c so-stdio.c -o so-stdio.o
clean:
	rm -rf *.o *.so