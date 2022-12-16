#include "kernelman.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "unistd.h"
#include "time.h"

#define ALLOCPAGES 985
#define SLOWALLOSPAGES 10

int main()
{
  // alloc
  printf("alloc %d pages in 1s\n", ALLOCPAGES);
  sleep(1);
  size_t ppn[ALLOCPAGES];
  for(int i = 0; i < ALLOCPAGES; i++)
    ppn[i] = allocPhysicalPage();


  size_t slow_ppn[SLOWALLOSPAGES];
  for(int i = 0; i < SLOWALLOSPAGES; i++)
  {
    printf("alloc page %d of %d in 1s\n", i+1, SLOWALLOSPAGES);
    sleep(1);
    slow_ppn[i] = allocPhysicalPage();
  }

  for(int i = 0; i < ALLOCPAGES; i++)
  {
    printf("free page %d of %d in 1s\n", i+1, SLOWALLOSPAGES);
    sleep(1);
    freePhysicalPage(slow_ppn[i]);
  }

  

  // free
  printf("free %d pages in 1s\n", ALLOCPAGES);
  sleep(1);
  for(int i = 0; i < ALLOCPAGES; i++)
    freePhysicalPage(ppn[i]);

  return 0;
}