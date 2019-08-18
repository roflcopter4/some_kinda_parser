#ifndef MY_P99_COMMON_H_
#define MY_P99_COMMON_H_

#include "Common.h"
#include "contrib/bstring/bstring.h"

#include "contrib/P99/p99.h"
#include "contrib/P99/p99_block.h"
#include "contrib/P99/p99_defarg.h"
#include "contrib/P99/p99_for.h"

#define pthread_create(...) P99_CALL_DEFARG(pthread_create, 4, __VA_ARGS__)
#define pthread_create_defarg_3() NULL

#define pthread_exit(...) P99_CALL_DEFARG(pthread_exit, 1, __VA_ARGS__)
#define pthread_exit_defarg_0() NULL

#define P44_ANDALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (&&), __VA_ARGS__)
#define P44_ORALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (||), __VA_ARGS__)

#define P04_EQ(WHAT, X, I) (X) == (WHAT)
#define P44_EQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_EQ, __VA_ARGS__)

#define P04_STREQ(WHAT, X, I) (strcmp((WHAT), (X)) == 0)
#define P44_STREQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_STREQ, __VA_ARGS__)

#define P04_B_ISEQ(WHAT, X, I) b_iseq((WHAT), (X))
#define P44_B_ISEQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_B_ISEQ, __VA_ARGS__)

#define P04_B_ISEQ_LIT(WHAT, X, I) (b_iseq((WHAT), (&(bstring){(sizeof(X) - 1), 0, (uchar *)("" X), 0})))
#define P44_B_ISEQ_LIT_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_B_ISEQ_LIT, __VA_ARGS__)

#define P44_DECLARE_FIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_FIFO_DECLARE(NAME##_ptr)

#define P44_DECLARE_LIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_LIFO_DECLARE(NAME##_ptr)

#define pipe2_throw(...) P99_THROW_CALL_NEG(pipe2, EINVAL, __VA_ARGS__)
#define dup2_throw(...)  P99_THROW_CALL_NEG(dup2, EINVAL, __VA_ARGS__)
#define execl_throw(...) P99_THROW_CALL_NEG(execl, EINVAL, __VA_ARGS__)

#define P01_FREE_BSTRING(BSTR) b_destroy(BSTR)
#define b_destroy_all(...) P99_BLOCK(P99_SEP(P01_FREE_BSTRING, __VA_ARGS__);)
#define P01_WRITEPROTECT_BSTRING(BSTR) b_writeprotect(BSTR)
#define b_writeprotect_all(...) P99_BLOCK(P99_SEP(P01_WRITEPROTECT_BSTRING, __VA_ARGS__);)
#define P01_WRITEALLOW_BSTRING(BSTR) b_writeallow(BSTR)
#define b_writeallow_all(...) P99_BLOCK(P99_SEP(P01_WRITEALLOW_BSTRING, __VA_ARGS__);)

#endif /* p99_common.h */
