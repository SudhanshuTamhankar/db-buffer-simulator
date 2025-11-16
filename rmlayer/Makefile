# Makefile for the RM (Record Management) Layer

# Compiler
CC = cc

# Flags
CFLAGS = -g -Wall -I$(PF_DIR)

# Path to the PF layer (one directory up)
PF_DIR = ../pflayer
PF_LIB = $(PF_DIR)/pflayer.o
PF_HDR = $(PF_DIR)/pf.h

# RM Layer files
RM_SRC = rm.c
RM_OBJ = rm.o
RM_HDR = rm.h rm_internal.h

# Test files
TEST_SRC = testrm.c
TEST_OBJ = testrm.o
TEST_EXEC = testrm

# Target to build the test executable
$(TEST_EXEC): $(TEST_OBJ) $(RM_OBJ) $(PF_LIB)
	$(CC) $(CFLAGS) -o $(TEST_EXEC) $(TEST_OBJ) $(RM_OBJ) $(PF_LIB)

# Rule to build the test object file
$(TEST_OBJ): $(TEST_SRC) $(RM_HDR) $(PF_HDR)
	$(CC) $(CFLAGS) -c $(TEST_SRC)

# Rule to build the RM object file
$(RM_OBJ): $(RM_SRC) $(RM_HDR) $(PF_HDR)
	$(CC) $(CFLAGS) -c $(RM_SRC)

# Default target
all: $(TEST_EXEC)

# Clean rule
clean:
	rm -f $(TEST_EXEC) *.o testfile.db