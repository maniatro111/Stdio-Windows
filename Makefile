build: so-stdio.obj
	link /nologo /dll /out:so_stdio.dll so-stdio.obj
so-stdio.obj: so-stdio.c
	cl /c so-stdio.c
clean:
	del so_stdio.exp so_stdio.dll so_stdio.lib so-stdio.obj