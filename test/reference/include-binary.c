#include <stdio.h>
#include <stdlib.h>

const char const message
[14] = { /* small-file.txt */
0xC2,0xA1,0x48,0x6F,0x6C,0x61,0x20,0x6D,0x75,0x6E,0x64,0x6F,0x21,0x0A
};

int main(int argc, char* argv[])
{
  // Can not use printf() because the message is not zero-terminated.
  // Use #embed instead to add bytes to the input, see
  // "include-binary-embed.c".
  fwrite(message, sizeof(message), sizeof(message[0]), stdout);

  return 0;
}
