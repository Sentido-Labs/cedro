#include <stdio.h>
#include <stdlib.h>

#pragma Cedro 1.0 #embed

const char const message[] = {
#embed "small-file.txt"
};

const char const message_string[] = {
#embed "small-file.txt"
  , 0 // Zero-terminator for the string.
};

int main(int argc, char* argv[])
{
  fwrite(message, sizeof(message), sizeof(message[0]), stdout);
  printf(message_string);

  return 0;
}
