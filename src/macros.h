#ifndef MACROS_DECLARE
#include "macros/backstitch.h"
#include "macros/defer.h"
#include "macros/slice.h"
#else
#define MACRO(name) { (MacroFunction_p) macro_##name, #name }
MACRO(backstitch),
MACRO(defer),
MACRO(slice),
#undef MACRO
#endif
