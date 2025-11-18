#PUBLICDIR= /usr0/cs564/public/project
SRC= buf.c hash.c pf.c
OBJ= buf.o hash.o pf.o
HDR = pftypes.h pf.h 

pflayer.o: $(OBJ)
	cc -r -o pflayer.o $(OBJ)

# This is the line that builds all test files
tests: testpf test_read_heavy test_write_heavy test_cyclic

testpf: testpf.o pflayer.o
	cc -o testpf testpf.o pflayer.o

test_read_heavy: test_read_heavy.o pflayer.o
	cc -o test_read_heavy test_read_heavy.o pflayer.o

test_write_heavy: test_write_heavy.o pflayer.o
	cc -o test_write_heavy test_write_heavy.o pflayer.o

test_cyclic: test_cyclic.o pflayer.o
	cc -o test_cyclic test_cyclic.o pflayer.o

$(OBJ): $(HDR)

testpf.o: $(HDR)
test_read_heavy.o: $(HDR)
test_write_heavy.o: $(HDR)
test_cyclic.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o

# Add this rule to the end
clean:
	rm -f *.o pflayer.o testpf test_read_heavy test_write_heavy test_cyclic file1 file2 read_heavy_file write_heavy_file cyclic_file data.csv