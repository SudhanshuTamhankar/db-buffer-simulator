/* testpf.c */
#include <stdio.h>
#include <stdlib.h> /* For exit() */
#include <string.h> /* For strcmp(), strncmp() */
#include "pf.h"

/* Add these definitions */
#define FILE1 "file1"
#define FILE2 "file2"

/* Add this internal constant */
#define PF_MAX_BUFS 20

/* Add internal function prototypes (for testing) */
void PFbufPrint(void);
void PFhashPrint(void);

/* Function Prototypes */
void writefile(const char *fname, int quiet);
void readfile(const char *fname, int quiet);
void printfile(int fd, int quiet);

/* Global quiet mode flag */
int g_quiet = 0;

/* Replacement for printf that respects quiet mode */
#define Q_PRINTF(...) \
  do { \
    if (!g_quiet) { \
      printf(__VA_ARGS__); \
    } \
  } while (0)

int main(int argc, char **argv) {
  int error;
  int i;
  int pagenum;
  char *buf;
  char *buf1, *buf2;
  int fd1, fd2;
  char strategy_choice[10];
  char strategy_name[4];

  /*
   * ==========================================================
   * RUNTIME SELECTION
   * ==========================================================
   */
  PF_Init(); /* Initialize the PF layer */

  /* Check for "quiet" command-line argument for graphing */
  if (argc > 1 && strcmp(argv[1], "-q") == 0) {
    g_quiet = 1;
  }

  /* Get strategy from user */
  if (!g_quiet) {
    printf("Select Page Replacement Strategy:\n");
    printf("  0. LRU (Least Recently Used)\n");
    printf("  1. MRU (Most Recently Used)\n");
    printf("Enter choice (0 or 1): ");
  }
  
  /* THIS IS THE PART YOU WERE MISSING */
  if (fgets(strategy_choice, sizeof(strategy_choice), stdin) == NULL) {
    if (!g_quiet) printf("Error reading input, defaulting to LRU\n");
    strcpy(strategy_choice, "1"); /* Default to LRU */
  }

  /* This logic uses "1" for LRU and "2" for MRU */
  if (strncmp(strategy_choice, "1", 1) == 0) {
    PF_SetStrategy(PF_MRU);
    strcpy(strategy_name, "MRU");
  } else {
    PF_SetStrategy(PF_LRU);
    strcpy(strategy_name, "LRU");
  }
  
  Q_PRINTF("\n*** STRATEGY SET TO %s ***\n\n", strategy_name);
  /* ==========================================================
   * END OF SELECTION CODE
   * ==========================================================
   */


  /* create a few files */
  if ((error = PF_CreateFile(FILE1)) != PFE_OK) {
    PF_PrintError("file1");
    exit(1);
  }
  Q_PRINTF("file1 created\n");

  if ((error = PF_CreateFile(FILE2)) != PFE_OK) {
    PF_PrintError("file2");
    exit(1);
  }
  Q_PRINTF("file2 created\n");

  /* write to file1 */
  writefile(FILE1, g_quiet);

  /* print it out */
  readfile(FILE1, g_quiet);

  /* write to file2 */
  writefile(FILE2, g_quiet);

  /* print it out */
  readfile(FILE2, g_quiet);

  /* open both files */
  if ((fd1 = PF_OpenFile(FILE1)) < 0) {
    PF_PrintError("open file1\n");
    exit(1);
  }
  Q_PRINTF("opened file1\n");

  if ((fd2 = PF_OpenFile(FILE2)) < 0) {
    PF_PrintError("open file2\n");
    exit(1);
  }
  Q_PRINTF("opened file2\n");

  /* get rid of records  1, 3, 5, etc from file 1,
  and 0,2,4,6 from file2 */
  for (i = 0; i < PF_MAX_BUFS; i++) {
    if (i & 1) {
      if ((error = PF_DisposePage(fd1, i)) != PFE_OK) {
        PF_PrintError("dispose\n");
        exit(1);
      }
      Q_PRINTF("disposed %d of file1\n", i);
    } else {
      if ((error = PF_DisposePage(fd2, i)) != PFE_OK) {
        PF_PrintError("dispose\n");
        exit(1);
      }
      Q_PRINTF("disposed %d of file2\n", i);
    }
  }

  if ((error = PF_CloseFile(fd1)) != PFE_OK) {
    PF_PrintError("close fd1");
    exit(1);
  }
  Q_PRINTF("closed file1\n");

  if ((error = PF_CloseFile(fd2)) != PFE_OK) {
    PF_PrintError("close fd2");
    exit(1);
  }
  Q_PRINTF("closed file2\n");
  /* print the files */
  readfile(FILE1, g_quiet);
  readfile(FILE2, g_quiet);

  /* destroy the two files */
  if ((error = PF_DestroyFile(FILE1)) != PFE_OK) {
    PF_PrintError("destroy file1");
    exit(1);
  }
  if ((error = PF_DestroyFile(FILE2)) != PFE_OK) {
    PF_PrintError("destroy file2");
    exit(1);
  }

  /* create them again */
  if ((fd1 = PF_CreateFile(FILE1)) < 0) {
    PF_PrintError("create file1");
    exit(1);
  }
  Q_PRINTF("file1 created\n");

  if ((fd2 = PF_CreateFile(FILE2)) < 0) {
    PF_PrintError("create file2");
    exit(1);
  }
  Q_PRINTF("file2 created\n");

  /* put stuff into the two files */
  writefile(FILE1, g_quiet);
  writefile(FILE2, g_quiet);

  /* Open the files, and see how the buffer manager
  handles more insertions, and deletions */
  /* open both files */
  if ((fd1 = PF_OpenFile(FILE1)) < 0) {
    PF_PrintError("open file1\n");
    exit(1);
  }
  Q_PRINTF("opened file1\n");

  if ((fd2 = PF_OpenFile(FILE2)) < 0) {
    PF_PrintError("open file2\n");
    exit(1);
  }
  Q_PRINTF("opened file2\n");

  for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
    if ((error = PF_AllocPage(fd2, &pagenum, &buf)) != PFE_OK) {
      PF_PrintError("first buffer\n");
      exit(1);
    }
    *((int *)buf) = i;
    if ((error = PF_UnfixPage(fd2, pagenum, TRUE)) != PFE_OK) {
      PF_PrintError("unfix file1");
      exit(1);
    }
    Q_PRINTF("alloc %d file1 page %d\n", i, pagenum);

    if ((error = PF_AllocPage(fd1, &pagenum, &buf)) != PFE_OK) {
      PF_PrintError("first buffer\n");
      exit(1);
    }
    *((int *)buf) = i;
    if ((error = PF_UnfixPage(fd1, pagenum, TRUE)) != PFE_OK) {
      PF_PrintError("dispose file1");
      exit(1);
    }
    Q_PRINTF("alloc %d file2 page %d\n", i, pagenum);
  }

  for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
    if (i & 1) {
      if ((error = PF_DisposePage(fd1, i)) != PFE_OK) {
        PF_PrintError("dispose fd1");
        exit(1);
      }
      Q_PRINTF("dispose fd1 page %d\n", i);
    } else {
      if ((error = PF_DisposePage(fd2, i)) != PFE_OK) {
        PF_PrintError("dispose fd2");
        exit(1);
      }
      Q_PRINTF("dispose fd2 page %d\n", i);
    }
  }

  Q_PRINTF("getting file2\n");
  for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
    if (i & 1) {
      if ((error = PF_GetThisPage(fd2, i, &buf)) != PFE_OK) {
        PF_PrintError("get this on fd2");
        exit(1);
      }
      Q_PRINTF("%d %d\n", i, *buf);
      if ((error = PF_UnfixPage(fd2, i, FALSE)) != PFE_OK) {
        PF_PrintError("get this on fd2");
        exit(1);
      }
    }
  }

  Q_PRINTF("getting file1\n");
  for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
    if (!(i & 1)) {
      if ((error = PF_GetThisPage(fd1, i, &buf)) != PFE_OK) {
        PF_PrintError("get this on fd2");
        exit(1);
      }
      Q_PRINTF("%d %d\n", i, *buf);
      if ((error = PF_UnfixPage(fd1, i, FALSE)) != PFE_OK) {
        PF_PrintError("get this on fd2");
        exit(1);
      }
    }
  }

  /* print the files */
  printfile(fd2, g_quiet);

  printfile(fd1, g_quiet);

  /*put some more stuff into file1 */
  Q_PRINTF("putting stuff into holes in fd1\n");
  for (i = 0; i < (PF_MAX_BUFS / 2 - 1); i++) {
    if (PF_AllocPage(fd1, &pagenum, &buf) != PFE_OK) {
      PF_PrintError("PF_AllocPage");
      exit(1);
    }
    *buf = pagenum;
    if (PF_UnfixPage(fd1, pagenum, TRUE) != PFE_OK) {
      PF_PrintError("PF_UnfixPage");
      exit(1);
    }
  }

  Q_PRINTF("printing fd1");
  printfile(fd1, g_quiet);

  PF_CloseFile(fd1);
  Q_PRINTF("closed file1\n");

  PF_CloseFile(fd2);
  Q_PRINTF("closed file2\n");

  /* open file1 twice */
  if ((fd1 = PF_OpenFile(FILE1)) < 0) {
    PF_PrintError("open file1");
    exit(1);
  }
  Q_PRINTF("opened file1\n");

  /* try to destroy it while it's still open*/
  error = PF_DestroyFile(FILE1);
  Q_PRINTF("destroy file1, should not succeed");
  if (!g_quiet) PF_PrintError("");

  /* get rid of some invalid page */
  error = PF_DisposePage(fd1, 100);
  Q_PRINTF("dispose page 100, should fail");
  if (!g_quiet) PF_PrintError("");

  /* get a valid page, and try to dispose it without unfixing.*/
  if ((error = PF_GetThisPage(fd1, 1, &buf)) != PFE_OK) {
    PF_PrintError("get this on fd2");
    exit(1);
  }
  Q_PRINTF("got page%d\n", *buf);
  error = PF_DisposePage(fd1, 1);
  Q_PRINTF("dispose page1, should fail");
  if (!g_quiet) PF_PrintError("");

  /* Now unfix it */
  if ((error = PF_UnfixPage(fd1, 1, FALSE)) != PFE_OK) {
    PF_PrintError("get this on fd2");
    exit(1);
  }

  error = PF_UnfixPage(fd1, 1, FALSE);
  Q_PRINTF("unfix fd1 again, should fail");
  if (!g_quiet) PF_PrintError("");

  if ((fd2 = PF_OpenFile(FILE1)) < 0) {
    PF_PrintError("open file1 again");
    exit(1);
  }
  Q_PRINTF("opened file1 again\n");

  printfile(fd1, g_quiet);

  printfile(fd2, g_quiet);

  if (PF_CloseFile(fd1) != PFE_OK) {
    PF_PrintError("close fd1");
    exit(1);
  }

  if (PF_CloseFile(fd2) != PFE_OK) {
    PF_PrintError("close fd2");
    exit(1);
  }

  Q_PRINTF("buffer:\n");
  if(!g_quiet) PFbufPrint();

  Q_PRINTF("hash table:\n");
  if(!g_quiet) PFhashPrint();

  /* Print the final CSV data */
  if (g_quiet) {
    /* THIS IS THE LINE THAT ADDS THE WORKLOAD NAME */
    printf("Balanced,%s,", strategy_name); /* Print "Balanced,LRU," */
    PF_PrintStats();                       /* Print "logical,physical,writes\n" */
  } else {
    printf("\n--- Final Statistics ---\n");
    PF_PrintStats();
  }

  return 0;
}

void writefile(const char *fname, int quiet) {
  int i;
  int fd, pagenum;
  char *buf;
  int error;

  if ((fd = PF_OpenFile((char *)fname)) < 0) {
    PF_PrintError("open file1");
    exit(1);
  }
  Q_PRINTF("opened %s\n", fname);

  for (i = 0; i < PF_MAX_BUFS; i++) {
    if ((error = PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK) {
      PF_PrintError("first buffer\n");
      exit(1);
    }
    *((int *)buf) = i;
    Q_PRINTF("allocated page %d\n", pagenum);
  }

  if ((error = PF_AllocPage(fd, &pagenum, &buf)) == PFE_OK) {
    Q_PRINTF("too many buffers, and it's still OK\n");
    exit(1);
  }

  for (i = 0; i < PF_MAX_BUFS; i++) {
    if ((error = PF_UnfixPage(fd, i, TRUE)) != PFE_OK) {
      PF_PrintError("unfix buffer\n");
      exit(1);
    }
  }

  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file1\n");
    exit(1);
  }
}

void readfile(const char *fname, int quiet) {
  int error;
  char *buf; /* Changed from int* */
  int pagenum;
  int fd;

  Q_PRINTF("opening %s\n", fname);
  if ((fd = PF_OpenFile((char *)fname)) < 0) {
    PF_PrintError("open file");
    exit(1);
  }
  printfile(fd, quiet);
  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file");
    exit(1);
  }
}

void printfile(int fd, int quiet) {
  int error;
  char *buf;
  int pagenum;

  Q_PRINTF("reading file\n");
  pagenum = -1;
  while ((error = PF_GetNextPage(fd, &pagenum, &buf)) == PFE_OK) {
    Q_PRINTF("got page %d, %d\n", pagenum, *((int *)buf));
    if ((error = PF_UnfixPage(fd, pagenum, FALSE)) != PFE_OK) {
      PF_PrintError("unfix");
      exit(1);
    }
  }
  if (error != PFE_EOF) {
    PF_PrintError("not eof\n");
    exit(1);
  }
  Q_PRINTF("eof reached\n");
}