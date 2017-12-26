CFLAGS=-O3 -march=native -g -Wall -Wextra

LIB:= -L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v9.0\lib\x64"
INC:= -I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v9.0\include" -I include

PROG=meshsmooth

SRC=src

output: $(SRC)/$(PROG).o
	g++ $(SRC)/$(PROG).o $(LIB) -lOpenCL -o $(PROG)

$(SRC)/$(PROG).o: $(SRC)/$(PROG).cpp
	g++ -c $(INC) $(SRC)/$(PROG).cpp -o $(SRC)/$(PROG).o

clean:
	del $(SRC)/*.o *.exe

.PHONY: clean