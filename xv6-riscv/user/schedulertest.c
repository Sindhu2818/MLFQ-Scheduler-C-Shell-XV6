#include "kernel/types.h"
#include "user/user.h"

void
worker(int id)
{
  volatile int i;

  printf("worker %d started\n", id);
  int limit;
if (id == 0) {
  limit = 500000000;
}
else if (id == 1) {
  limit = 700000000;
}
else if (id == 2) {
  limit = 900000000;
}
else {
  limit = 1200000000;
}
for (i = 0; i < limit; i++) {
  if (i % 10000000 == 0)
    printf("worker %d checkpoint\n", id);
}
  printf("worker %d finished\n", id);
  exit(0);
}
int
main(void)
{
  int i;
  int pid;

  printf("MLFQ scheduler test\n");
  for(i = 0; i < 4; i++) {
    pid = fork();

    if(pid == 0)
      worker(i);
  }

  for(i = 0; i < 4; i++)
    wait(0);

  printf("MLFQ scheduler test finished\n");

  exit(0);
}