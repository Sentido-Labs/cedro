#ifndef MACROS_DECLARE
#include "macros/backstitch.h"
#include "macros/defer.h"
#else
#define MACRO(name) { (MacroFunction_p) macro_##name, #name }
MACRO(backstitch),
MACRO(defer),
#undef MACRO
#endif
