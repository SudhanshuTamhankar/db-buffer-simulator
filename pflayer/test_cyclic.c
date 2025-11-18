/* test_cyclic.c - Cyclic access pattern that shows MRU benefit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"

#define FILENAME "cyclic_file"
#define NUM_PAGES 25        /* More than buffer (20), so eviction happens */
#define NUM_CYCLES 100      /* Repeat the cycle many times */

int g_quiet = 0;

#define Q_PRINTF(...) \
  do { \
    if (!g_quiet) { \
      printf(__VA_ARGS__); \
    } \
  } while (0)

int main(int argc, char **argv) {
  int error, i, cycle;
  int pagenum;
  char *buf;
  int fd;
  char strategy_choice[10];
  char strategy_name[4];

  PF_Init();

  if (argc > 1 && strcmp(argv[1], "-q") == 0) {
    g_quiet = 1;
  }

  if (!g_quiet) {
    printf("Select Page Replacement Strategy:\n");
    printf("  0. LRU (Least Recently Used)\n");
    printf("  1. MRU (Most Recently Used)\n");
    printf("Enter choice (0 or 1): ");
  }
  
  if (fgets(strategy_choice, sizeof(strategy_choice), stdin) == NULL) {
    strcpy(strategy_choice, "1");
  }

  if (strncmp(strategy_choice, "1", 1) == 0) {
    PF_SetStrategy(PF_MRU);
    strcpy(strategy_name, "MRU");
  } else {
    PF_SetStrategy(PF_LRU);
    strcpy(strategy_name, "LRU");
  }
  
  Q_PRINTF("\n*** STRATEGY SET TO %s ***\n\n", strategy_name);

  /* Create file and write initial pages */
  if ((error = PF_CreateFile(FILENAME)) != PFE_OK) {
    PF_PrintError("create file");
    exit(1);
  }
  if ((fd = PF_OpenFile(FILENAME)) < 0) {
    PF_PrintError("open file");
    exit(1);
  }

  Q_PRINTF("Writing %d pages (buffer holds 20)...\n", NUM_PAGES);
  for (i = 0; i < NUM_PAGES; i++) {
    if ((error = PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK) {
      PF_PrintError("alloc page");
      exit(1);
    }
    *((int *)buf) = i;
    if ((error = PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK) {
      PF_PrintError("unfix page");
      exit(1);
    }
  }

  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file");
    exit(1);
  }

  /* Reopen and perform cyclic access pattern */
  if ((fd = PF_OpenFile(FILENAME)) < 0) {
    PF_PrintError("open file for test");
    exit(1);
  }

  Q_PRINTF("Performing cyclic access (0->24, repeat %d times)...\n", NUM_CYCLES);
  Q_PRINTF("LRU will evict oldest on each cycle, causing many disk reads.\n");
  Q_PRINTF("MRU will keep the cycle in buffer better.\n\n");

  for (cycle = 0; cycle < NUM_CYCLES; cycle++) {
    /* Access pages in sequence 0, 1, 2, ..., 24 */
    for (i = 0; i < NUM_PAGES; i++) {
      if ((error = PF_GetThisPage(fd, i, &buf)) != PFE_OK) {
        PF_PrintError("get this page");
        exit(1);
      }
      if ((error = PF_UnfixPage(fd, i, FALSE)) != PFE_OK) {
        PF_PrintError("unfix page");
        exit(1);
      }
    }
  }

  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    PF_PrintError("close file");
    exit(1);
  }

  if ((error = PF_DestroyFile(FILENAME)) != PFE_OK) {
    PF_PrintError("destroy file");
    exit(1);
  }

  if (g_quiet) {
    printf("Cyclic,%s,", strategy_name);
    PF_PrintStats();
  } else {
    printf("\n--- Final Statistics (Cyclic Access) ---\n");
    PF_PrintStats();
  }

  return 0;
}
