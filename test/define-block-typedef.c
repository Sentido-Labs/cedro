#pragma Cedro 1.0
#define { MUT_CONST_TYPE_VARIANTS(T)
/*  */      mut_##T,
    *       mut_##T##_mut_p,
    * const mut_##T##_p;
typedef const mut_##T T,
    *                 T##_mut_p,
    * const           T##_p
#define }
