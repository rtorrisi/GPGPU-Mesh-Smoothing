CFLAGS=-O3 -march=native -g -Wall -Wextra

LIB=-L"C:\Intel\OpenCL\sdk\lib\x64"
INC=-I"C:\Intel\OpenCL\sdk\include"

PROG=vecsmooth

output: $(PROG).o
	gcc $(PROG).o $(LIB) -lOpenCL -o $(PROG)

$(PROG).o: $(PROG).c
	gcc -c $(INC) $(PROG).c

clean:
	del *.o *.exe

.PHONY: clean