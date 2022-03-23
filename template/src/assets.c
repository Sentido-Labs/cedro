/** Binary assets, like icons, fonts, etc. */

#include <stdlib.h> // size_t
#include <stdint.h> // uint8_t

#pragma Cedro 1.0

const uint8_t icon
#include {../logo.png}
;
const size_t sizeof_icon = sizeof(icon);

const uint8_t font_sans
#include {../fonts/b612-1.008/B612Mono-Regular.ttf}
;
const size_t sizeof_font_sans = sizeof(font_sans);
