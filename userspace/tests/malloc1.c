#include "stdlib.h"
#include "stdio.h"

int main()
{
  // Be creative ;)

  // Check all the requirements from our assignment description:
  // https://www.iaik.tugraz.at/teaching/materials/slp/assignments/a5/
  // Try to test as many as you can!
  // If you don't know how to test for something, you can ask for help
  // via discord or mail.
  printf("starting my old slp testcase now...\n");
  int* data[5];
  int sizes[5] = {100 , 1020, 1014, 3075, 200};
  data[0] = (int*)malloc(sizes[0] * sizeof(int));
  data[1] = (int*)malloc(sizes[1] * sizeof(int));
  data[2] = (int*)malloc(sizes[2] * sizeof(int));
  data[3] = (int*)malloc(sizes[3] * sizeof(int));
  data[4] = (int*)malloc(sizes[4] * sizeof(int));
  for (size_t i = 0; i < 5; i++)
  {
    if(data[i] == NULL)
    {
      printf("something went wrong with malloc\n");
      return -1;
    }
    for (size_t j = 0; j < sizes[i]; j++)
    {
      data[i][j] = 576;
    }
  }
  free(data[3]);
  free(data[4]);
  
  for (size_t j = 0; j < 3; j++)
  {
    free(data[j]);
  }
  printf("freed everything and exit the programm sucressfully!\n");
  
  // You can add new files in /tests/, c and c++ both work.
  return 0;
}
