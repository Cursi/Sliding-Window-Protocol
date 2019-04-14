#include "link_emulator/lib.h"

#define REAL_MESSAGE_SIZE (MSGSIZE - 13)
#define DELAY_RATE 0.5

/*
  Magical type numbers
  1 - fileName
  2 - numberOfPackages
  3 - data package
*/

typedef struct
{
  char type;
  int index;
  int hash;
  char message[REAL_MESSAGE_SIZE];
} Payload;