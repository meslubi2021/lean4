/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <iostream>
#include <sstream>
#include "util/test.h"
#include "kernel/environment.h"
#include "kernel/abstract.h"
#include "kernel/formatter.h"
#include "library/printer.h"
using namespace lean;

static void check(format const & f, char const * expected) {
    std::ostringstream strm;
    strm << f;
    std::cout << strm.str() << "\n";
    lean_assert_eq(strm.str(), expected);
}

static void tst1() {
    environment env;
    env->add_var("N", Type());
    formatter fmt = mk_simple_formatter();
    check(fmt(env), "variable N : Type\n");
    expr f  = Const("f");
    expr a  = Const("a");
    expr x  = Const("x");
    expr y  = Const("y");
    expr N  = Const("N");
    expr F  = Fun({x, Pi({x, N}, x >> x)}, Let({y, f(a)}, f(HEq(x, f(y, a)))));
    check(fmt(F), "fun x : (Pi x : N, (x#0 -> x#1)), (let y := f a in (f (x#1 == (f y#0 a))))");
    check(fmt(env->get_object("N")), "variable N : Type");
    context ctx;
    ctx = extend(ctx, "x", f(a));
    ctx = extend(ctx, "y", f(Var(0), N >> N));
    ctx = extend(ctx, "z", N, HEq(Var(0), Var(1)));
    check(fmt(ctx), "[x : f a; y : f x#0 (N -> N); z : N := y#0 == x#1]");
    check(fmt(ctx, f(Var(0), Var(2))), "f z#0 x#2");
    check(fmt(ctx, f(Var(0), Var(2)), true), "[x : f a; y : f x#0 (N -> N); z : N := y#0 == x#1] |- f z#0 x#2");
}

int main() {
    save_stack_info();
    tst1();
    return has_violations() ? 1 : 0;
}
