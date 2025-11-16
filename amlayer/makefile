# Makefile for the AM (Access Method) Layer

# Compiler
CC = cc

# Flags
# -I../pflayer tells the compiler to look in the pflayer dir for pf.h
# -I../rmlayer tells the compiler to look in the rmlayer dir for rm.h
CFLAGS = -g -Wall -I../pflayer -I../rmlayer

# --- PF Layer ---
PF_LIB = ../pflayer/pflayer.o

# --- RM Layer ---
RM_LIB = ../rmlayer/rm.o

# --- AM Layer ---
# Source files for the AM layer, including misc.c
AM_SRC = am.c amfns.c amglobals.c aminsert.c amprint.c amscan.c amsearch.c amstack.c misc.c
AM_OBJ = $(AM_SRC:.c=.o)

# --- Test Executable ---
# We build 'testam' from 'test_objective3.c'
TEST_EXEC = testam
TEST_SRC = test_objective3.c
TEST_OBJ = test_objective3.o

# --- Build Rules ---

# Default target: build the test executable
all: $(TEST_EXEC)

# Link the final executable
$(TEST_EXEC): $(TEST_OBJ) $(AM_OBJ) $(RM_LIB) $(PF_LIB)
	$(CC) $(CFLAGS) -o $(TEST_EXEC) $(TEST_OBJ) $(AM_OBJ) $(RM_LIB) $(PF_LIB)

# --- Dependencies ---
# Tell make that all .o files depend on the headers
# This lets make use its built-in .c -> .o rules
$(TEST_OBJ) $(AM_OBJ): am.h testam.h ../rmlayer/rm.h ../pflayer/pf.h

# Clean rule
clean:
	rm -f $(TEST_EXEC) *.o *.0 testrel.0 student_records.db student_records.0