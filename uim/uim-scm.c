/*

  Copyright (c) 2003-2007 uim Project http://uim.freedesktop.org/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

/*
 * To avoid namespace pollution, all SigScheme functions and variables
 * are defined as static and wrapped into uim-scm.c by direct
 * inclusion instead of being linked via public symbols.
 *   -- YamaKen 2004-12-21, 2005-01-10, 2006-04-02
 */
/* This file must be included before uim's config.h */
#include "sigscheme-combined.c"
#if !SSCM_VERSION_REQUIRE(0, 8, 0)
#error "SigScheme version 0.8.0 or later is required"
#endif

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "uim-scm.h"
#include "uim-internal.h"
/* To avoid macro name conflict with SigScheme, uim-scm-abbrev.h should not
 * be included. */

#ifdef UIM_COMPAT_SCM
#include "uim-compat-scm.c"
#endif

/* FIXME: illegal internal access */
#define scm_out SCM_GLOBAL_VAR(port, scm_out)
#define scm_err SCM_GLOBAL_VAR(port, scm_err)

static uim_lisp protected;
static uim_bool initialized, sscm_is_exit_with_fatal_error;
static FILE *uim_output = NULL;

static void uim_scm_error(const char *msg, uim_lisp errobj);
struct uim_scm_error_args {
  const char *msg;
  uim_lisp errobj;
};
static void *uim_scm_error_internal(struct uim_scm_error_args *args);

struct call_args {
  uim_lisp proc;
  uim_lisp args;
  uim_lisp failed;
};
static void *uim_scm_call_internal(struct call_args *args);
static void *uim_scm_call_with_guard_internal(struct call_args *args);

struct callf_args {
  const char *proc;
  const char *args_fmt;
  va_list args;
  uim_bool with_guard;
  uim_lisp failed;
};
static void *uim_scm_callf_internal(struct callf_args *args);

static void *uim_scm_c_int_internal(void *uim_lisp_integer);
static void *uim_scm_make_int_internal(void *integer);
static const char *uim_scm_refer_c_str_internal(void *uim_lisp_str);
static void *uim_scm_make_str_internal(const char *str);
static void *uim_scm_make_symbol_internal(const char *name);
static void *uim_scm_make_ptr_internal(void *ptr);
static void *uim_scm_make_func_ptr_internal(uim_func_ptr func_ptr);
static void *uim_scm_symbol_value_internal(const char *symbol_str);
static void *uim_scm_symbol_value_int_internal(const char *symbol_str);
static char *uim_scm_symbol_value_str_internal(const char *symbol_str);
static void *uim_scm_eval_internal(void *uim_lisp_obj);
static void *uim_scm_quote_internal(void *obj);
struct cons_args {
  uim_lisp car;
  uim_lisp cdr;
};
static void *uim_scm_cons_internal(struct cons_args *args);

static void
uim_scm_error(const char *msg, uim_lisp errobj)
{
  struct uim_scm_error_args args;

  assert(uim_scm_gc_any_contextp());
  assert(msg);
  assert(uim_scm_gc_protectedp(errobj));

  args.msg = msg;
  args.errobj = errobj;
  uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_error_internal, &args);
}

static void *
uim_scm_error_internal(struct uim_scm_error_args *args)
{
  /* FIXME: don't terminate the process */
  scm_error_obj(NULL, args->msg, (ScmObj)args->errobj);
  SCM_NOTREACHED;
}

FILE *
uim_scm_get_output(void)
{
  assert(uim_scm_gc_any_contextp());

  return uim_output;
}

void
uim_scm_set_output(FILE *fp)
{
  assert(uim_scm_gc_any_contextp());

  uim_output = fp;
}

void
uim_scm_ensure(uim_bool cond)
{
  assert(uim_scm_gc_protected_contextp());

  SCM_ENSURE(cond);
}

uim_bool
uim_scm_c_bool(uim_lisp val)
{
  assert(uim_scm_gc_any_contextp());

  return UIM_SCM_NFALSEP(val);
}

uim_lisp
uim_scm_make_bool(uim_bool val)
{
  assert(uim_scm_gc_any_contextp());

  return (val) ? uim_scm_t() : uim_scm_f();
}

long
uim_scm_c_int(uim_lisp integer)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(integer));

  return (long)(intptr_t)uim_scm_call_with_gc_ready_stack(uim_scm_c_int_internal, (void *)integer);
}

static void *
uim_scm_c_int_internal(void *uim_lisp_integer)
{
  long c_int;
  uim_lisp integer;

  integer = (uim_lisp)uim_lisp_integer;

  if (!SCM_INTP((ScmObj)integer))
    uim_scm_error("uim_scm_c_int: number required but got ",
                  (uim_lisp)integer);

  c_int = SCM_INT_VALUE((ScmObj)integer);
  return (void *)(intptr_t)c_int;
}

uim_lisp
uim_scm_make_int(long integer)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)uim_scm_call_with_gc_ready_stack(uim_scm_make_int_internal,
                                                    (void *)(intptr_t)integer);
}

static void *
uim_scm_make_int_internal(void *integer)
{
  return (void *)SCM_MAKE_INT((intptr_t)integer);
}

char *
uim_scm_c_str(uim_lisp str)
{
  const char *c_str;

  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(str));

  c_str = uim_scm_refer_c_str(str);

  return (c_str) ? strdup(c_str) : NULL;
}

const char *
uim_scm_refer_c_str(uim_lisp str)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(str));

  return uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_refer_c_str_internal, (void *)str);
}

static const char *
uim_scm_refer_c_str_internal(void *uim_lisp_str)
{
  char *c_str;
  uim_lisp str;

  str = (uim_lisp)uim_lisp_str;

  if (SCM_STRINGP((ScmObj)str)) {
    c_str = SCM_STRING_STR((ScmObj)str);
  } else if (SCM_SYMBOLP((ScmObj)str)) {
    c_str = SCM_SYMBOL_NAME((ScmObj)str);
  } else {
    uim_scm_error("uim_scm_refer_c_str: string or symbol required but got ",
                  (uim_lisp)str);
    SCM_NOTREACHED;
  }

  return c_str;
}

uim_lisp
uim_scm_make_str(const char *str)
{
  assert(uim_scm_gc_any_contextp());
  assert(str);

  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_make_str_internal, (void *)str);
}

static void *
uim_scm_make_str_internal(const char *str)
{
  return (void *)SCM_MAKE_STRING_COPYING(str, SCM_STRLEN_UNKNOWN);
}

char *
uim_scm_c_symbol(uim_lisp symbol)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(symbol));

  return strdup((char *)SCM_SYMBOL_NAME((ScmObj)symbol));
}

uim_lisp
uim_scm_make_symbol(const char *name)
{
  assert(uim_scm_gc_any_contextp());
  assert(name);

  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_make_symbol_internal, (void *)name);
}

static void *
uim_scm_make_symbol_internal(const char *name)
{
  return (void *)scm_intern(name);
}

void *
uim_scm_c_ptr(uim_lisp ptr)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(ptr));

  if (!SCM_C_POINTERP((ScmObj)ptr))
    uim_scm_error("uim_scm_c_ptr: C pointer required but got ", (uim_lisp)ptr);

  return SCM_C_POINTER_VALUE((ScmObj)ptr);
}

uim_lisp
uim_scm_make_ptr(void *ptr)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)uim_scm_call_with_gc_ready_stack(uim_scm_make_ptr_internal,
                                                    ptr);
}

static void *
uim_scm_make_ptr_internal(void *ptr)
{
  return (void *)SCM_MAKE_C_POINTER(ptr);
}

uim_func_ptr
uim_scm_c_func_ptr(uim_lisp func_ptr)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(func_ptr));

  if (!SCM_C_FUNCPOINTERP((ScmObj)func_ptr))
    uim_scm_error("uim_scm_c_func_ptr: C function pointer required but got ",
                  (uim_lisp)func_ptr);

  return SCM_C_FUNCPOINTER_VALUE((ScmObj)func_ptr);
}

uim_lisp
uim_scm_make_func_ptr(uim_func_ptr func_ptr)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_make_func_ptr_internal, (void *)(uintptr_t)func_ptr);
}

static void *
uim_scm_make_func_ptr_internal(uim_func_ptr func_ptr)
{
  return (void *)SCM_MAKE_C_FUNCPOINTER((ScmCFunc)func_ptr);
}

void
uim_scm_gc_protect(uim_lisp *location)
{
  assert(uim_scm_gc_any_contextp());
  assert(location);

  scm_gc_protect((ScmObj *)location);
}

void
uim_scm_gc_unprotect(uim_lisp *location)
{
  assert(uim_scm_gc_any_contextp());
  assert(location);

  scm_gc_unprotect((ScmObj *)location);
}

void *
uim_scm_call_with_gc_ready_stack(uim_gc_gate_func_ptr func, void *arg)
{
  assert(uim_scm_gc_any_contextp());
  assert(func);

  return scm_call_with_gc_ready_stack(func, arg);
}

uim_bool
uim_scm_gc_protectedp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return scm_gc_protectedp((ScmObj)obj);
}

uim_bool
uim_scm_gc_protected_contextp(void)
{
  return (initialized && scm_gc_protected_contextp());
}

uim_bool
uim_scm_is_alive(void)
{
  assert(uim_scm_gc_any_contextp());

  return (!sscm_is_exit_with_fatal_error);
}

long
uim_scm_get_verbose_level(void)
{
  assert(uim_scm_gc_any_contextp());

  return (long)scm_get_verbose_level();
}

void
uim_scm_set_verbose_level(long new_value)
{
  assert(uim_scm_gc_any_contextp());

  scm_set_verbose_level(new_value);
}

void
uim_scm_set_lib_path(const char *path)
{
  assert(uim_scm_gc_any_contextp());

  scm_set_lib_path(path);
}

/* temporary solution for getting an value from Scheme world */
uim_lisp
uim_scm_symbol_value(const char *symbol_str)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_symbol_value_internal, (void *)symbol_str);
}

static void *
uim_scm_symbol_value_internal(const char *symbol_str)
{
  ScmObj symbol;

  symbol = scm_intern(symbol_str);
  if (SCM_TRUEP(scm_p_symbol_boundp(symbol, SCM_NULL))) {
    return (void *)(uim_lisp)scm_p_symbol_value(symbol);
  } else {
    return (void *)uim_scm_f();
  }
}

uim_bool
uim_scm_symbol_value_bool(const char *symbol_str)
{
  uim_bool val;

  assert(uim_scm_gc_any_contextp());

  if (!symbol_str)
    return UIM_FALSE;

  val = uim_scm_c_bool(uim_scm_symbol_value(symbol_str));

  return val;
}

int
uim_scm_symbol_value_int(const char *symbol_str)
{
  assert(uim_scm_gc_any_contextp());

  return (int)(intptr_t)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_symbol_value_int_internal, (void *)symbol_str);
}

static void *
uim_scm_symbol_value_int_internal(const char *symbol_str)
{
  uim_lisp val_;
  int val;

  val_ = uim_scm_symbol_value(symbol_str);

  if (UIM_SCM_NFALSEP(val_)) {
    val = uim_scm_c_int(val_);
  } else {
    val = 0;
  }

  return (void *)(intptr_t)val;
}

char *
uim_scm_symbol_value_str(const char *symbol_str)
{
  assert(uim_scm_gc_any_contextp());

  return uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_symbol_value_str_internal, (void *)symbol_str);
}

static char *
uim_scm_symbol_value_str_internal(const char *symbol_str)
{
  uim_lisp val_;
  char *val;

  val_ = uim_scm_symbol_value(symbol_str);

  if (UIM_SCM_NFALSEP(val_)) {
    val = uim_scm_c_str(val_);
  } else {
    val = NULL;
  }

  return val;
}

uim_bool
uim_scm_load_file(const char *fn)
{
  uim_lisp ok;

  assert(uim_scm_gc_any_contextp());

  if (!fn)
    return UIM_FALSE;

  /* (guard (err (else #f)) (load "<fn>")) */
  protected = ok = uim_scm_callf_with_guard(uim_scm_f(), "load", "s", fn);

  return uim_scm_c_bool(ok);
}

uim_lisp
uim_scm_t(void)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)SCM_TRUE;
}

uim_lisp
uim_scm_f(void)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)SCM_FALSE;
}

uim_lisp
uim_scm_null(void)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)SCM_NULL;
}

uim_lisp
uim_scm_quote(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(obj));

  return (uim_lisp)uim_scm_call_with_gc_ready_stack(uim_scm_quote_internal,
                                                    (void *)obj);
}

static void *
uim_scm_quote_internal(void *obj)
{
  return (void *)SCM_LIST_2(SCM_SYM_QUOTE, (ScmObj)obj);
}

uim_lisp
uim_scm_list1(uim_lisp elm1)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(elm1));

  return uim_scm_cons(elm1, uim_scm_null_list());
}

uim_lisp
uim_scm_list2(uim_lisp elm1, uim_lisp elm2)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(elm1));
  assert(uim_scm_gc_protectedp(elm2));

  return uim_scm_cons(elm1, uim_scm_list1(elm2));
}

uim_lisp
uim_scm_list3(uim_lisp elm1, uim_lisp elm2, uim_lisp elm3)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(elm1));
  assert(uim_scm_gc_protectedp(elm2));
  assert(uim_scm_gc_protectedp(elm3));

  return uim_scm_cons(elm1, uim_scm_list2(elm2, elm3));
}

uim_lisp
uim_scm_list4(uim_lisp elm1, uim_lisp elm2, uim_lisp elm3, uim_lisp elm4)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(elm1));
  assert(uim_scm_gc_protectedp(elm2));
  assert(uim_scm_gc_protectedp(elm3));
  assert(uim_scm_gc_protectedp(elm4));

  return uim_scm_cons(elm1, uim_scm_list3(elm2, elm3, elm4));
}

uim_lisp
uim_scm_list5(uim_lisp elm1, uim_lisp elm2, uim_lisp elm3, uim_lisp elm4,
              uim_lisp elm5)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(elm1));
  assert(uim_scm_gc_protectedp(elm2));
  assert(uim_scm_gc_protectedp(elm3));
  assert(uim_scm_gc_protectedp(elm4));
  assert(uim_scm_gc_protectedp(elm5));

  return uim_scm_cons(elm1, uim_scm_list4(elm2, elm3, elm4, elm5));
}

uim_bool
uim_scm_nullp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return (SCM_NULLP((ScmObj)obj));
}

uim_bool
uim_scm_consp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return (SCM_CONSP((ScmObj)obj));
}

uim_bool
uim_scm_integerp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return (SCM_INTP((ScmObj)obj));
}

uim_bool
uim_scm_stringp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return (SCM_STRINGP((ScmObj)obj));
}

uim_bool
uim_scm_symbolp(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());

  return (SCM_SYMBOLP((ScmObj)obj));
}

uim_bool
uim_scm_eq(uim_lisp a, uim_lisp b)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(a));
  assert(uim_scm_gc_protectedp(b));

  return (SCM_EQ((ScmObj)a, (ScmObj)b));
}

uim_lisp
uim_scm_eval(uim_lisp obj)
{
  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(obj));

  return (uim_lisp)uim_scm_call_with_gc_ready_stack(uim_scm_eval_internal,
						    (void *)obj);
}

static void *
uim_scm_eval_internal(void *uim_lisp_obj)
{
  uim_lisp obj;

  obj = (uim_lisp)uim_lisp_obj;

  return (void *)scm_p_eval((ScmObj)obj, SCM_NULL);
}

uim_lisp
uim_scm_eval_c_string(const char *str)
{
  assert(uim_scm_gc_any_contextp());

  return (uim_lisp)scm_eval_c_string(str);
}

uim_lisp
uim_scm_call(uim_lisp proc, uim_lisp args)
{
  struct call_args _args;

  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(proc));
  assert(uim_scm_gc_protectedp(args));

  _args.proc = proc;
  _args.args = args;
  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_call_internal, &_args);
}

static void *
uim_scm_call_internal(struct call_args *args)
{
  if (uim_scm_symbolp(args->proc))
    args->proc = uim_scm_eval(args->proc);

  return (void *)scm_call((ScmObj)args->proc, (ScmObj)args->args);
}

uim_lisp
uim_scm_call_with_guard(uim_lisp failed, uim_lisp proc, uim_lisp args)
{
  struct call_args _args;

  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(failed));
  assert(uim_scm_gc_protectedp(proc));
  assert(uim_scm_gc_protectedp(args));

  _args.failed = failed;
  _args.proc = proc;
  _args.args = args;
  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_call_with_guard_internal, &_args);
}

static void *
uim_scm_call_with_guard_internal(struct call_args *args)
{
  uim_lisp form;

  /* (guard (err (else '<failed>)) (apply <proc> '<args>)) */
  form = uim_scm_list3(uim_scm_make_symbol("guard"),
                       uim_scm_list2(uim_scm_make_symbol("err"),
                                     uim_scm_list2(uim_scm_make_symbol("else"),
                                                   uim_scm_quote(args->failed))),
                       uim_scm_list3(uim_scm_make_symbol("apply"),
                                     args->proc,
                                     uim_scm_quote(args->args)));

  return (void *)uim_scm_eval(form);
}

uim_lisp
uim_scm_callf(const char *proc, const char *args_fmt, ...)
{
  uim_lisp ret;
  struct callf_args args;

  assert(uim_scm_gc_any_contextp());
  assert(proc);
  assert(args_fmt);

  va_start(args.args, args_fmt);

  args.proc = proc;
  args.args_fmt = args_fmt;
  args.with_guard = UIM_FALSE;
  ret = (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_callf_internal, &args);

  va_end(args.args);

  return ret;
}

static void *
uim_scm_callf_internal(struct callf_args *args)
{
  ScmObj proc, scm_args, arg;
  ScmQueue argq;
  const char *fmtp;

  proc = scm_eval(scm_intern(args->proc), SCM_INTERACTION_ENV);
  scm_args = SCM_NULL;
  SCM_QUEUE_POINT_TO(argq, scm_args);
  for (fmtp = args->args_fmt; *fmtp; fmtp++) {
    switch (*fmtp) {
    case 'b':
      arg = SCM_MAKE_BOOL(va_arg(args->args, int));
      break;

    case 'i':
      arg = SCM_MAKE_INT(va_arg(args->args, int));
      break;

    case 'j':
      arg = SCM_MAKE_INT(va_arg(args->args, intmax_t));
      break;

    /* FIXME: enable R6RS chars by default */
#if SCM_USE_CHAR
    case 'c':
      arg = SCM_MAKE_CHAR(va_arg(args->args, int));
      break;
#endif

    case 's':
      arg = SCM_MAKE_STRING_COPYING(va_arg(args->args, const char *),
                                    SCM_STRLEN_UNKNOWN);
      break;

    case 'y':
      arg = scm_intern(va_arg(args->args, const char *));
      break;

    case 'p':
      arg = SCM_MAKE_C_POINTER(va_arg(args->args, void *));
      break;

    case 'f':
      arg = SCM_MAKE_C_FUNCPOINTER(va_arg(args->args, ScmCFunc));
      break;

    case 'o':
      arg = (ScmObj)va_arg(args->args, uim_lisp);
      assert(scm_gc_protectedp(arg));
      break;

    default:
      assert(scm_false);
    }
    SCM_QUEUE_ADD(argq, arg);
  }

  if (args->with_guard)
    return (void *)uim_scm_call_with_guard(args->failed,
                                           (uim_lisp)proc, (uim_lisp)scm_args);
  else
    return (void *)(uim_lisp)scm_call(proc, scm_args);
}

uim_lisp
uim_scm_callf_with_guard(uim_lisp failed,
                         const char *proc, const char *args_fmt, ...)
{
  uim_lisp ret;
  struct callf_args args;

  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(failed));
  assert(proc);
  assert(args_fmt);

  va_start(args.args, args_fmt);

  args.proc = proc;
  args.args_fmt = args_fmt;
  args.with_guard = UIM_TRUE;
  args.failed = failed;
  ret = (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_callf_internal, &args);

  va_end(args.args);

  return ret;
}

uim_lisp
uim_scm_car(uim_lisp pair)
{
  assert(uim_scm_gc_protected_contextp());

  return (uim_lisp)scm_p_car((ScmObj)pair);
}

uim_lisp
uim_scm_cdr(uim_lisp pair)
{
  assert(uim_scm_gc_protected_contextp());

  return (uim_lisp)scm_p_cdr((ScmObj)pair);
}

uim_lisp
uim_scm_cons(uim_lisp car, uim_lisp cdr)
{
  struct cons_args args;

  assert(uim_scm_gc_any_contextp());
  assert(uim_scm_gc_protectedp(car));
  assert(uim_scm_gc_protectedp(cdr));

  args.car = car;
  args.cdr = cdr;
  return (uim_lisp)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_scm_cons_internal, &args);
}

static void *
uim_scm_cons_internal(struct cons_args *args)
{
  return (void *)SCM_CONS((ScmObj)args->car, (ScmObj)args->cdr);
}

long
uim_scm_length(uim_lisp lst)
{
  uim_lisp len;

  assert(uim_scm_gc_protected_contextp());
  assert(uim_scm_gc_protectedp(lst));

  protected = len = (uim_lisp)scm_p_length((ScmObj)lst);
  return uim_scm_c_int(len);
}

uim_bool
uim_scm_require_file(const char *fn)
{
  uim_lisp ok;

  assert(uim_scm_gc_any_contextp());

  if (!fn)
    return UIM_FALSE;

  /* (guard (err (else #f)) (require "<fn>")) */
  protected = ok = uim_scm_callf_with_guard(uim_scm_f(), "require", "s", fn);

  return uim_scm_c_bool(ok);
}

void
uim_scm_init_subr_0(const char *name, uim_lisp (*func)(void))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_0)func, SCM_PROCEDURE_FIXED_0);
}

void
uim_scm_init_subr_1(const char *name, uim_lisp (*func)(uim_lisp))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_1)func, SCM_PROCEDURE_FIXED_1);
}

void
uim_scm_init_subr_2(const char *name, uim_lisp (*func)(uim_lisp, uim_lisp))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_2)func, SCM_PROCEDURE_FIXED_2);
}

void
uim_scm_init_subr_3(const char *name,
		    uim_lisp (*func)(uim_lisp, uim_lisp, uim_lisp))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_3)func, SCM_PROCEDURE_FIXED_3);
}

void
uim_scm_init_subr_4(const char *name,
		    uim_lisp (*func)(uim_lisp, uim_lisp, uim_lisp, uim_lisp))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_4)func, SCM_PROCEDURE_FIXED_4);
}

void
uim_scm_init_subr_5(const char *name,
		    uim_lisp (*func)(uim_lisp, uim_lisp, uim_lisp, uim_lisp,
				     uim_lisp))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_procedure_fixed_5)func, SCM_PROCEDURE_FIXED_5);
}

void
uim_scm_init_fsubr(const char *name,
                   uim_lisp (*func)(uim_lisp args, uim_lisp env))
{
  assert(uim_scm_gc_protected_contextp());
  assert(name);
  assert(func);

  scm_register_func(name, (scm_syntax_variadic_0)func, SCM_SYNTAX_VARIADIC_0);
}

static void
exit_hook(void)
{
  sscm_is_exit_with_fatal_error = UIM_TRUE;
  /* FIXME: Add longjmp() to outermost uim API call, and make all API
   * calls uim_scm_is_alive()-sensitive. It should be fixed on uim
   * 1.5.  -- YamaKen 2006-06-06, 2006-12-27 */
}

void
uim_scm_init(const char *verbose_level)
{
  ScmStorageConf storage_conf;
  long vlevel = 2;
  ScmObj output_port;

  if (initialized)
    return;

  if (!uim_output)
    uim_output = stderr;

  if (verbose_level && isdigit(verbose_level[0])) {
    vlevel = atoi(verbose_level) % 10;
  }

#if SCM_USE_MULTIBYTE_CHAR
  /* *GC safe operation*
   * 
   * Set the raw unibyte codec which accepts all (multi)byte sequence
   * although it slashes a multibyte character on Scheme-level
   * character processing. Since current uim implementation treats a
   * multibyte character as string, it is not a problem. The name
   * "ISO-8859-1" is a dummy name for the codec.
   */
  scm_current_char_codec = scm_mb_find_codec("ISO-8859-1");
#endif

  /* 128KB/heap, max 0.99GB on 32-bit systems. Since maximum length of list can
   * be represented by a Scheme integer, SCM_INT_MAX limits the number of cons
   * cells. */
  storage_conf.heap_size            = 16384;
  storage_conf.heap_alloc_threshold = 16384;
  storage_conf.n_heaps_max          = SCM_INT_MAX / storage_conf.heap_size;
  storage_conf.n_heaps_init         = 1;
  storage_conf.symbol_hash_size     = 1024;
  scm_initialize(&storage_conf);
  scm_set_fatal_error_callback(exit_hook);

  /* GC safe */
  output_port = scm_make_shared_file_port(uim_output, "uim", SCM_PORTFLAG_OUTPUT);
  scm_out = scm_err = output_port;

  protected = (uim_lisp)SCM_FALSE;
  uim_scm_gc_protect(&protected);

#ifdef DEBUG_SCM
  /* required by test-im.scm */
  uim_scm_callf("provide", "s", "debug");
#endif

  scm_use("srfi-23");
  scm_use("srfi-34");
  scm_use("siod");

  uim_scm_set_verbose_level(vlevel);
  initialized = UIM_TRUE;
}

void
uim_scm_quit(void)
{
  scm_finalize();
  sscm_is_exit_with_fatal_error = UIM_FALSE;
  uim_output = NULL;
  initialized = UIM_FALSE;
}
