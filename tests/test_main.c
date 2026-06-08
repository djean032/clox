#include "test.h"

#include <string.h>
#include <time.h>

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

extern int registerMemoryArenaTests(TestCase *tests, int max);
extern int registerMemoryHeapTests(TestCase *tests, int max);

static bool parseArgs(int argc, char **argv, bool *verbose) {
  *verbose = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
      *verbose = true;
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[i]);
      return false;
    }
  }

  return true;
}

static double elapsedMs(struct timespec start, struct timespec end) {
  time_t seconds = end.tv_sec - start.tv_sec;
  long nanoseconds = end.tv_nsec - start.tv_nsec;

  return (double)seconds * 1000.0 + (double)nanoseconds / 1000000.0;
}

static int collectTests(TestCase *tests, int max) {
  int count = 0;

  count += registerMemoryArenaTests(tests + count, max - count);
  count += registerMemoryHeapTests(tests + count, max - count);

  return count;
}

int main(int argc, char **argv) {
  bool verbose;
  if (!parseArgs(argc, argv, &verbose)) {
    return 1;
  }

  TestCase tests[256];
  int count = collectTests(tests, 256);
  int failures = 0;
  struct timespec totalStart;
  struct timespec totalEnd;

  if (verbose) {
    printf("test count: %d\n", count);
  }

  clock_gettime(CLOCK_MONOTONIC, &totalStart);

  for (int i = 0; i < count; i++) {
    struct timespec testStart;
    struct timespec testEnd;

    if (verbose) {
      printf("RUN  %s\n", tests[i].name);
    }

    clock_gettime(CLOCK_MONOTONIC, &testStart);
    bool passed = tests[i].fn();
    clock_gettime(CLOCK_MONOTONIC, &testEnd);

    double testElapsed = elapsedMs(testStart, testEnd);
    if (passed) {
      printf(COLOR_GREEN "PASS" COLOR_RESET " %s %.3f ms\n", tests[i].name,
             testElapsed);
    } else {
      printf(COLOR_RED "FAIL" COLOR_RESET " %s %.3f ms\n", tests[i].name,
             testElapsed);
      failures++;
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &totalEnd);
  double totalElapsed = elapsedMs(totalStart, totalEnd);

  if (failures == 0) {
    printf("%d tests, " COLOR_GREEN "%d failures" COLOR_RESET
           ", %.3f ms total\n",
           count, failures, totalElapsed);
  } else {
    printf("%d tests, " COLOR_RED "%d failures" COLOR_RESET
           ", %.3f ms total\n",
           count, failures, totalElapsed);
  }

  return failures == 0 ? 0 : 1;
}
