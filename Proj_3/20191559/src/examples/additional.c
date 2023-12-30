/* Added in #Proj 1. */

#include <stdio.h>
#include <syscall.h>
#include <stdlib.h>


int main(int argc, char *argv[]) {
  if(argc != 5) {
      printf("Usage: ./additional [num 1] [num 2] [num 3] [num 4]\n");
      return EXIT_FAILURE;
  }
  
  printf("%d %d\n", FIBONACCI(atoi(argv[1])), 
    MAX_OF_FOUR_INT(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4])));

  return EXIT_SUCCESS;
}