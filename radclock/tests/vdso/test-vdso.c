#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

// Manual definition of ffclock_getcounter
extern int ffclock_getcounter(uint64_t *ffcounter);

int main(int argc, char **argv)
{

  int result = 0;
  uint64_t ffcounter_test = 0;

  result = ffclock_getcounter(&ffcounter_test);
  printf("result: %i\n", result);
  printf("counter: %" PRIu64 "\n", ffcounter_test);

  return 0;
}
