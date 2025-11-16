/* am.h: Public Interface for the Access Method (AM) Layer */
#ifndef AM_H
#define AM_H

/* Standard C Library Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include PF and RM layer headers */
#include "pf.h"
#include "rm.h"

/*
 * =================================================================
 * Type Definitions
 * =================================================================
 */

typedef struct am_leafheader {
  char pageType;      /* 'l' for leaf */
  int nextLeafPage;   /* Page number of next leaf, or AM_NULL_PAGE */
  short recIdPtr;     /* Offset to start of free space for recIds */
  short keyPtr;       /* Offset to start of free space for keys */
  short freeListPtr;  /* Offset to head of free list for recIds */
  short numinfreeList;/* Number of entries in free list */
  short attrLength;   /* Length of the attribute (key) */
  short numKeys;      /* Number of keys on the page */
  short maxKeys;      /* Max keys this page can hold */
} AM_LEAFHEADER;      /* Header for a leaf page */

typedef struct am_intheader {
  char pageType;   /* 'i' for internal */
  short numKeys;   /* Number of keys on the page */
  short maxKeys;   /* Max keys this page can hold */
  short attrLength;/* Length of the attribute (key) */
} AM_INTHEADER;    /* Header for an internal node */

/*
 * =================================================================
 * Global Variables
 * =================================================================
 */

extern int AM_RootPageNum; /* The page number of the root */
extern int AM_LeftPageNum; /* The page Number of the leftmost leaf */
extern int AM_Errno;       /* last error in AM layer */

/*
 * =================================================================
 * Definitions and Constants
 * =================================================================
 */

/* Macro to check PF layer errors */
#define AM_Check(errVal) \
  if ((errVal) != PFE_OK) { \
    AM_Errno = AME_PF; \
    return (AME_PF); \
  }

/* Type sizes */
#define AM_si sizeof(int)
#define AM_ss sizeof(short)
#define AM_sl sizeof(AM_LEAFHEADER)
#define AM_sint sizeof(AM_INTHEADER)
#define AM_sc sizeof(char)
#define AM_sf sizeof(float)

/* Search status */
#define AM_NOT_FOUND 0 /* Key is not in tree */
#define AM_FOUND 1     /* Key is in tree */

/* Misc constants */
#define AM_NULL 0            /* Null pointer for lists in a page */
#define AM_MAX_FNAME_LENGTH 80
#define AM_NULL_PAGE -1
#define AM_MAXATTRLENGTH 256

/* Scan Table status */
#define FREE 0
#define FIRST 1
#define BUSY 2
#define LAST 3
#define OVER 4

/* Scan operators */
#define ALL 0
#define EQUAL 1
#define LESS_THAN 2
#define GREATER_THAN 3
#define LESS_THAN_EQUAL 4
#define GREATER_THAN_EQUAL 5
#define NOT_EQUAL 6
#define MAXSCANS 20

/*
 * =================================================================
 * AM Layer Error Codes
 * =================================================================
 */
#define AME_OK 0
#define AME_INVALIDATTRLENGTH -1
#define AME_NOTFOUND -2
#define AME_PF -3
#define AME_INTERROR -4
#define AME_INVALID_SCANDESC -5
#define AME_INVALID_OP_TO_SCAN -6
#define AME_EOF -7
#define AME_SCAN_TAB_FULL -8
#define AME_INVALIDATTRTYPE -9
#define AME_FD -10
#define AME_INVALIDVALUE -11

/*
 * =================================================================
 * Public Function Prototypes (from amfns.c, aminsert.c, amscan.c)
 * =================================================================
 */

/* amfns.c */
int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength);
int AM_DestroyIndex(char *fileName, int indexNo);
int AM_DeleteEntry(int fileDesc, char attrType, int attrLength, char *value, int recId);
int AM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value, int recId);
void AM_PrintError(char *s);

/* amscan.c */
int AM_OpenIndexScan(int fileDesc, char attrType, int attrLength, int op, char *value);
int AM_FindNextEntry(int scanDesc);
int AM_CloseIndexScan(int scanDesc);

/*
 * =================================================================
 * Internal Function Prototypes (used between AM files)
 * =================================================================
 */

/* am.c */
int AM_SplitLeaf(int fileDesc, char *pageBuf, int *pageNum, int attrLength, int recId, char *value, int status, int index, char *key);
int AM_AddtoParent(int fileDesc, int pageNum, char *value, int attrLength);
void AM_AddtoIntPage(char *pageBuf, char *value, int pageNum, AM_INTHEADER *header, int offset);
void AM_FillRootPage(char *pageBuf, int pageNum1, int pageNum2, char *value, short attrLength, short maxKeys);
void AM_SplitIntNode(char *pageBuf, char *pbuf1, char *pbuf2, AM_INTHEADER *header, char *value, int pageNum, int offset);

/* aminsert.c */
int AM_InsertintoLeaf(char *pageBuf, int attrLength, char *value, int recId, int index, int status);
void AM_InsertToLeafFound(char *pageBuf, int recId, int index, AM_LEAFHEADER *header);
void AM_InsertToLeafNotFound(char *pageBuf, char *value, int recId, int index, AM_LEAFHEADER *header);
void AM_Compact(int low, int high, char *pageBuf, char *tempPage, AM_LEAFHEADER *header);

/* amprint.c */
void AM_PrintIntNode(char *pageBuf, char attrType);
void AM_PrintLeafNode(char *pageBuf, char attrType);
void AM_DumpLeafPages(int fileDesc, int min, char attrType, int attrLength);
void AM_PrintLeafKeys(char *pageBuf, char attrType);
void AM_PrintAttr(char *bufPtr, char attrType, int attrLength);
void AM_PrintTree(int fileDesc, int pageNum, char attrType);

/* amscan.c */
int GetLeftPageNum(int fileDesc);

/* amsearch.c */
int AM_Search(int fileDesc, char attrType, int attrLength, char *value, int *pageNum, char **pageBuf, int *indexPtr);
int AM_BinSearch(char *pageBuf, char attrType, int attrLength, char *value, int *indexPtr, AM_INTHEADER *header);
int AM_SearchLeaf(char *pageBuf, char attrType, int attrLength, char *value, int *indexPtr, AM_LEAFHEADER *header);
int AM_Compare(char *bufPtr, char attrType, int attrLength, char *valPtr);

/* amstack.c */
void AM_PushStack(int pageNum, int offset);
void AM_PopStack(void);
void AM_topofStack(int *pageNum, int *offset);
void AM_EmptyStack(void);

#endif /* AM_H */