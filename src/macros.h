#ifndef MACROS_DECLARE
#include "macros/backstitch.h"
#include "macros/defer.h"
#include "macros/slice.h"
#include "macros/self.h"
#else
#define MACRO(name) { (MacroFunction_p) macro_##name, #name }
MACRO(backstitch),
MACRO(defer),
MACRO(slice),
MACRO(self),
#undef MACRO
#endif
