CFLAGS=-O3 -march=native -g -Wall -Wextra

LIB:= -L"lib\x64"
INC:= -I include

PROG=meshsmooth

SRC=src

$(PROG).exe: $(SRC)/$(PROG).o $(SRC)/ocl_boiler.o
	g++ -O3 $(SRC)/$(PROG).o $(SRC)/ocl_boiler.o $(LIB) -lOpenCL -o $(PROG)

$(SRC)/$(PROG).o: $(SRC)/$(PROG).cpp
	g++ -O3 -c $(INC) $(SRC)/$(PROG).cpp -o $(SRC)/$(PROG).o

$(SRC)/ocl_boiler.o: $(SRC)/ocl_boiler.cpp
	g++ -O3 -c $(INC) $(SRC)/ocl_boiler.cpp -o $(SRC)/ocl_boiler.o


execute:$(PROG).exe
	$(PROG).exe 

clean:
	del $(SRC)\*.o *.exe

.PHONY: clean execute