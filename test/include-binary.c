#include <stdio.h>
#include <stdlib.h>

#pragma Cedro 1.0

const char const message
#include {small-file.txt}
;

int main(int argc, char* argv[])
{
  // Can not use printf() because the message is not zero-terminated.
  // Use #embed instead to add bytes to the input, see
  // "include-binary-embed.c".
  fwrite(message, sizeof(message), sizeof(message[0]), stdout);

  return 0;
}
