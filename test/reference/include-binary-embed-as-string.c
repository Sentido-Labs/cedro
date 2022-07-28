#include <stdio.h>
#include <stdlib.h>

const char const message[14] =
/* small-file.txt */
"\302\241Hola mundo!\n";

const char const message_string[15] =
/* small-file.txt */
"\302\241Hola mundo!\n";

int main(int argc, char* argv[])
{
  fwrite(message, sizeof(message), sizeof(message[0]), stdout);
  printf(message_string);

  return 0;
}
