/* testam.h: Header file for AM layer tests */
#ifndef TESTAM_H
#define TESTAM_H

/* Constants from the original test files */
#define RELNAME "testrel"
#define RECNAME_INDEXNO 0
#define RECVAL_INDEXNO 1
#define NAMELENGTH 10

/* Type definitions from the original test files */
#define INT_TYPE 'i'
#define CHAR_TYPE 'c'
#define FLOAT_TYPE 'f'
typedef int RecIdType;

/* Macro to convert RecIdType to int */
#define RecIdToInt(recid) (recid)
#define IntToRecId(recid) (recid)

/* Operators from am.h (for test files) */
#define EQ_OP EQUAL
#define LT_OP LESS_THAN
#define GT_OP GREATER_THAN
#define LE_OP LESS_THAN_EQUAL
#define GE_OP GREATER_THAN_EQUAL
#define NE_OP NOT_EQUAL

/* Function prototypes from misc.c */
void padstring(char *str, int length);
int xAM_CreateIndex(char *fname, int indexno, char attrtype, int attrlen);
int xAM_DestroyIndex(char *fname, int indexno);
int xAM_InsertEntry(int fd, char attrtype, int attrlen, char *val, RecIdType recid);
int xAM_DeleteEntry(int fd, char attrtype, int attrlen, char *val, RecIdType recid);
int xAM_OpenIndexScan(int fd, char attrtype, int attrlen, int op, char *val);
RecIdType xAM_FindNextEntry(int sd);
int xAM_CloseIndexScan(int sd);
int xPF_OpenFile(char *fname);
int xPF_CloseFile(int fd);

#endif /* TESTAM_H */