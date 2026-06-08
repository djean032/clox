#ifndef CLOX_TEST_H
#define CLOX_TEST_H

#include <stdbool.h>
#include <stdio.h>

typedef bool (*TestFn)(void);

typedef struct {
  const char *name;
  TestFn fn;
} TestCase;

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);          \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define ADD_TEST(name)                                                         \
  do {                                                                         \
    if (count < max) {                                                         \
      tests[count++] = (TestCase){#name, name};                                \
    }                                                                          \
  } while (0)

#endif
