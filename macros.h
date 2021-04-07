#ifndef MACROS_DECLARE
#include "macros/fn.h"
#include "macros/let.h"
#include "macros/function_pointer.h"
#include "macros/backstitch.h"
#include "macros/count_markers.h"
#include "macros/collect_typedefs.h"
#else
#define MACRO(name) { (MacroFunction_p) macro_##name, #name }
MACRO(fn),
MACRO(let),
MACRO(function_pointer),
MACRO(backstitch),
MACRO(count_markers),
MACRO(collect_typedefs),
#undef MACRO
#endif
