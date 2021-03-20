shell: shell.o io_redirection.o search_path.o exec.o parse.o colors.o
	gcc -o shell shell.o colors.o io_redirection.o search_path.o exec.o parse.o constants.h
	rm -rf *.o
	rm -rf *.h.gch

shell.o: shell.c
	gcc -c shell.c

io_redirection.o: io_redirection.c io_redirection.h
	gcc -c io_redirection.c io_redirection.h

search_path.o: search_path.c search_path.h
	gcc -c search_path.c search_path.h

exec.o: exec.c exec.h
	gcc -c exec.c exec.h

parse.o: parse.c parse.h
	gcc -c parse.c parse.h

colors.o: colors.c colors.h
	gcc -c colors.c colors.h
