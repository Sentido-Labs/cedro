/** Binary assets, like icons, fonts, etc. */

#include <stdlib.h> // size_t
#include <stdint.h> // uint8_t

// Remove the Cedro pragma in this file if your compiler implements C23.
// The compiler will probably include the binary much faster by itself
// because it does not need to
// convert the bytes to numeric literals and parse them back up.
#pragma Cedro 1.0 #embed

const uint8_t icon[] = {
#embed "../logo.png"
};
const size_t sizeof_icon = sizeof(icon);

const uint8_t font_sans[] = {
#embed "../fonts/b612-1.008/B612Mono-Regular.ttf"
};
const size_t sizeof_font_sans = sizeof(font_sans);
