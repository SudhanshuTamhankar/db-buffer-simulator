/* test_read_heavy.c */
#include <stdio.h>
#include <stdlib.h> /* For exit(), rand(), srand() */
#include <string.h> /* For strcmp(), strncmp() */
#include <time.h>   /* For time() */
#include "pf.h"

#define FILENAME "read_heavy_file"
#define NUM_PAGES 100
#define NUM_READS (NUM_PAGES * 100) /* 100 reads per page */

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
  int fd;
  char strategy_choice[10];
  char strategy_name[4];

  /* Seed the random number generator */
  srand(time(NULL));

  PF_Init(); /* Initialize the PF layer */

  /* Check for "quiet" command-line argument for graphing */
  if (argc > 1 && strcmp(argv[1], "-q") == 0) {
    g_quiet = 1;
  }

  /* Get strategy from user */
  if (!g_quiet) {
    printf("Select Page Replacement Strategy:\n");
    printf("  1. LRU (Least Recently Used)\n");
    printf("  2. MRU (Most Recently Used)\n");
    printf("Enter choice (1 or 2): ");
  }
  
  if (fgets(strategy_choice, sizeof(strategy_choice), stdin) == NULL) {
    if (!g_quiet) printf("Error reading input, defaulting to LRU\n");
    strcpy(strategy_choice, "1"); /* Default to LRU */
  }

  if (strncmp(strategy_choice, "2", 1) == 0) {
    PF_SetStrategy(PF_MRU);
    strcpy(strategy_name, "MRU");
  } else {
    PF_SetStrategy(PF_LRU);
    strcpy(strategy_name, "LRU");
  }
  
  Q_PRINTF("\n*** STRATEGY SET TO %s ***\n\n", strategy_name);

  /*
   * ==========================================================
   * PHASE 1: SETUP (Write pages to disk)
   * ==========================================================
   */
  if ((error = PF_CreateFile(FILENAME)) != PFE_OK) {
    PF_PrintError("create file");
    exit(1);
  }
  if ((fd = PF_OpenFile(FILENAME)) < 0) {
    PF_PrintError("open file");
    exit(1);
  }

  Q_PRINTF("Writing %d setup pages...\n", NUM_PAGES);
  for (i = 0; i < NUM_PAGES; i++) {
    if ((error = PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK) {
      PF_PrintError("alloc page");
      exit(1);
    }
    *((int *)buf) = i; /* Write page number as content */
    if ((error = PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK) {
      PF_PrintError("unfix page");
      exit(1);
    }
  }

  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file");
    exit(1);
  }

  /*
   * ==========================================================
   * PHASE 2: TEST (Read pages randomly)
   * ==========================================================
   */
  if ((fd = PF_OpenFile(FILENAME)) < 0) {
    PF_PrintError("open file for read test");
    exit(1);
  }

  Q_PRINTF("Performing %d random reads...\n", NUM_READS);
  for (i = 0; i < NUM_READS; i++) {
    int page_to_read = rand() % NUM_PAGES;
    if ((error = PF_GetThisPage(fd, page_to_read, &buf)) != PFE_OK) {
      PF_PrintError("get this page");
      exit(1);
    }
    
    /* Verify content */
    if (*((int *)buf) != page_to_read) {
        printf("Data error on page %d! Expected %d, got %d\n",
               page_to_read, page_to_read, *((int *)buf));
    }

    /* Unfix as *NOT* dirty */
    if ((error = PF_UnfixPage(fd, page_to_read, FALSE)) != PFE_OK) {
      PF_PrintError("unfix read page");
      exit(1);
    }
  }

  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file after read test");
    exit(1);
  }

  /*
   * ==========================================================
   * PHASE 3: CLEANUP & STATS
   * ==========================================================
   */
  if ((error = PF_DestroyFile(FILENAME)) != PFE_OK) {
    PF_PrintError("destroy file");
    exit(1);
  }

  /* Print the final CSV data */
  if (g_quiet) {
    printf("ReadHeavy,%s,", strategy_name); /* Print "ReadHeavy,LRU," */
    PF_PrintStats();                        /* Print "logical,physical,writes\n" */
  } else {
    printf("\n--- Final Statistics (Read-Heavy) ---\n");
    PF_PrintStats();
  }

  return 0;
}