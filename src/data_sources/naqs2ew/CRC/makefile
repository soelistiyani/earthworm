all: testcrc crctable testcrc32

testcrc: crcmodel.o testcrc.o
	$(CC) -o testcrc testcrc.o crcmodel.o

crctable: crcmodel.o crctable.o
	$(CC) -o crctable crctable.o crcmodel.o

testcrc32: testcrc32.o crc32.o
	$(CC) -o testcrc32 testcrc32.o crc32.o
