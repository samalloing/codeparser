
#pragma once

#if USE_MATHLINK
#include "mathlink.h"
#undef P
#endif // USE_MATHLINK

#include "WolframLibrary.h"
#undef True
#undef False

using expr = void *;

//
// From expr library
//
EXTERN_C mint Expr_Length(expr);

EXTERN_C mint Expr_ToInteger(expr);

EXTERN_C expr Expr_FromInteger(mint);

EXTERN_C expr Expr_LookupSymbol(const char *);

EXTERN_C expr Expr_BuildExpression0(expr);

EXTERN_C expr Expr_BuildExpression1(expr, expr);

EXTERN_C expr Expr_BuildExpression2(expr, expr, expr);

EXTERN_C expr Expr_Evaluate(expr);

EXTERN_C expr Expr_BuildExpression(expr, mint);

EXTERN_C void Expr_Insert(expr, mint, expr);

EXTERN_C mint Expr_Pointer(expr);

EXTERN_C expr Expr_FromPointer(mint);

// EXTERN_C void Expr_DecrementRefCount(expr);



#if USE_MATHLINK

EXTERN_C DLLEXPORT int ExprTest_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res);

#endif // USE_MATHLINK