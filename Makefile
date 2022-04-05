build: so-stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib so-stdio.obj
so-stdio.obj: so-stdio.c
	cl /c so-stdio.c
clean:
	rm -rf *.o *.so