CFLAGS=-O3 -march=native -g -Wall -Wextra

LIB:= -L"lib\x64"
INC:= -I include

PROG=meshsmooth

SRC=src

$(PROG).exe: $(SRC)/$(PROG).o
	g++ $(SRC)/$(PROG).o $(LIB) -lOpenCL -o $(PROG)

$(SRC)/$(PROG).o: $(SRC)/$(PROG).cpp
	g++ -c $(INC) $(SRC)/$(PROG).cpp -o $(SRC)/$(PROG).o

execute:$(PROG).exe
	$(PROG).exe 

clean:
	del $(SRC)/*.o *.exe

.PHONY: clean execute