# amlayer/Makefile - builds AM test program and ensures PF & RM are built

CC = cc
CFLAGS = -g -Wall -I../pflayer -I../rmlayer

# Paths to subprojects (we call their make)
PF_DIR = ../pflayer
RM_DIR = ../rmlayer

# Objects (AM)
AM_SRC = am.c amfns.c amglobals.c aminsert.c amprint.c amscan.c amsearch.c amstack.c misc.c
AM_OBJ = $(AM_SRC:.c=.o)

TEST_EXEC = testam
TEST_SRC = test_objective3.c
TEST_OBJ = $(TEST_SRC:.c=.o)

# PF and RM library object files (built by their make)
PF_LIB = $(PF_DIR)/pflayer.o
RM_LIB = $(RM_DIR)/rm.o

.PHONY: all pf rm clean

all: pf rm $(TEST_EXEC)

# Build PF layer (calls make in pflayer)
pf:
	@$(MAKE) -C $(PF_DIR)

# Build RM layer (calls make in rmlayer)
rm:
	@$(MAKE) -C $(RM_DIR)

# Link the final executable
$(TEST_EXEC): $(TEST_OBJ) $(AM_OBJ) $(RM_LIB) $(PF_LIB)
	$(CC) $(CFLAGS) -o $(TEST_EXEC) $(TEST_OBJ) $(AM_OBJ) $(RM_LIB) $(PF_LIB)

# Let make build .o from .c using defaults but ensure headers are noted
$(TEST_OBJ) $(AM_OBJ): am.h testam.h ../rmlayer/rm.h ../pflayer/pf.h

clean:
	@$(MAKE) -C $(PF_DIR) clean || true
	@$(MAKE) -C $(RM_DIR) clean || true
	-rm -f $(TEST_EXEC) *.o
