/* SPDX-License-Identifier: MIT
 * Missing iOS libm/number parsing imports. Every floating argument/return crosses
 * the Darwin base AAPCS boundary explicitly, then calls Vita's hard-float libc.
 */
#include <math.h>
#include <stdlib.h>
#include "imports.h"
SOFTFP static double b_acos(double x) { return acos(x); }
SOFTFP static double b_asin(double x) { return asin(x); }
SOFTFP static float b_asinf(float x) { return asinf(x); }
SOFTFP static double b_atan(double x) { return atan(x); }
SOFTFP static float b_atanf(float x) { return atanf(x); }
SOFTFP static float b_atan2f(float y,float x) { return atan2f(y,x); }
SOFTFP static double b_ceil(double x) { return ceil(x); }
SOFTFP static float b_ceilf(float x) { return ceilf(x); }
SOFTFP static float b_coshf(float x) { return coshf(x); }
SOFTFP static double b_exp(double x) { return exp(x); }
SOFTFP static double b_fmax(double x,double y) { return fmax(x,y); }
SOFTFP static double b_fmin(double x,double y) { return fmin(x,y); }
SOFTFP static float b_fmodf(float x,float y) { return fmodf(x,y); }
SOFTFP static double b_frexp(double x,int *exp) { return frexp(x,exp); }
SOFTFP static double b_log(double x) { return log(x); }
SOFTFP static double b_log2(double x) { return log2(x); }
SOFTFP static double b_modf(double x,double *integral) { return modf(x,integral); }
SOFTFP static float b_roundf(float x) { return roundf(x); }
SOFTFP static float b_sinhf(float x) { return sinhf(x); }
SOFTFP static double b_tan(double x) { return tan(x); }
SOFTFP static float b_tanhf(float x) { return tanhf(x); }
SOFTFP static double b_atof(const char *p) { return atof(p); }
SOFTFP static double b_strtod(const char *p,char **end) { return strtod(p,end); }
const import_entry math_bridge_imports[] = {
 F(acos,b_acos), F(asin,b_asin), F(asinf,b_asinf), F(atan,b_atan),
 F(atanf,b_atanf), F(atan2f,b_atan2f), F(ceil,b_ceil), F(ceilf,b_ceilf),
 F(coshf,b_coshf), F(exp,b_exp), F(fmax,b_fmax), F(fmin,b_fmin),
 F(fmodf,b_fmodf), F(frexp,b_frexp), F(log,b_log), F(log2,b_log2),
 F(modf,b_modf), F(roundf,b_roundf), F(sinhf,b_sinhf), F(tan,b_tan),
 F(tanhf,b_tanhf), F(atof,b_atof), F(strtod,b_strtod),
};
const int math_bridge_imports_count=sizeof(math_bridge_imports)/sizeof(math_bridge_imports[0]);
