/*
Copyright (c) 2015 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "util/sexpr/option_declarations.h"
#include "library/constants.h"
#include "library/util.h"
#include "library/tactic/expr_to_tactic.h"
#include "library/tactic/relation_tactics.h"
#include "library/simplifier/simp_tactic.h"

#ifndef LEAN_DEFAULT_SIMP_SINGLE_PASS
#define LEAN_DEFAULT_SIMP_SINGLE_PASS false
#endif

#ifndef LEAN_DEFAULT_SIMP_BOTTOM_UP
#define LEAN_DEFAULT_SIMP_BOTTOM_UP true
#endif

#ifndef LEAN_DEFAULT_SIMP_BETA_ETA
#define LEAN_DEFAULT_SIMP_BETA_ETA true
#endif

#ifndef LEAN_DEFAULT_SIMP_IOTA
#define LEAN_DEFAULT_SIMP_IOTA true
#endif

#ifndef LEAN_DEFAULT_SIMP_MEMOIZE
#define LEAN_DEFAULT_SIMP_MEMOIZE true
#endif

#ifndef LEAN_DEFAULT_SIMP_MAX_STEPS
#define LEAN_DEFAULT_SIMP_MAX_STEPS 10000
#endif

#ifndef LEAN_DEFAULT_SIMP_TRACE
#define LEAN_DEFAULT_SIMP_TRACE false
#endif

#ifndef LEAN_DEFAULT_SIMP_ASSUMPTIONS
#define LEAN_DEFAULT_SIMP_ASSUMPTIONS false
#endif

namespace lean {
name const * g_simp_single_pass = nullptr;
name const * g_simp_bottom_up   = nullptr;
name const * g_simp_beta_eta    = nullptr;
name const * g_simp_iota        = nullptr;
name const * g_simp_memoize     = nullptr;
name const * g_simp_max_steps   = nullptr;
name const * g_simp_trace       = nullptr;
name const * g_simp_assumptions = nullptr;

bool get_simp_single_pass(options const & opts) {
    return opts.get_bool(*g_simp_single_pass, LEAN_DEFAULT_SIMP_SINGLE_PASS);
}

bool get_simp_bottom_up(options const & opts) {
    return opts.get_bool(*g_simp_bottom_up, LEAN_DEFAULT_SIMP_BOTTOM_UP);
}

bool get_simp_beta_eta(options const & opts) {
    return opts.get_bool(*g_simp_beta_eta, LEAN_DEFAULT_SIMP_BETA_ETA);
}

bool get_simp_iota(options const & opts) {
    return opts.get_bool(*g_simp_iota, LEAN_DEFAULT_SIMP_IOTA);
}

bool get_simp_memoize(options const & opts) {
    return opts.get_bool(*g_simp_memoize, LEAN_DEFAULT_SIMP_MEMOIZE);
}

unsigned get_simp_max_steps(options const & opts) {
    return opts.get_bool(*g_simp_max_steps, LEAN_DEFAULT_SIMP_MAX_STEPS);
}

bool get_simp_trace(options const & opts) {
    return opts.get_bool(*g_simp_trace, LEAN_DEFAULT_SIMP_TRACE);
}

bool get_simp_assumptions(options const & opts) {
    return opts.get_bool(*g_simp_assumptions, LEAN_DEFAULT_SIMP_ASSUMPTIONS);
}

expr const * g_simp_tactic = nullptr;

expr mk_simp_tactic_expr(buffer<expr> const & ls, buffer<name> const & ns,
                         buffer<name> const & ex, optional<expr> const & pre_tac,
                         location const & loc) {
    expr e  = mk_expr_list(ls.size(), ls.data());
    expr n  = ids_to_tactic_expr(ns);
    expr x  = ids_to_tactic_expr(ex);
    expr t;
    if (pre_tac) {
        t = mk_app(mk_constant(get_option_some_name()), *pre_tac);
    } else {

        t = mk_constant(get_option_none_name());
    }
    expr l = mk_location_expr(loc);
    expr r = mk_app({*g_simp_tactic, e, n, x, t, l});
    return r;
}

class simp_tactic_fn {
    environment      m_env;
    io_state         m_ios;
    name_generator   m_ngen;
    optional<tactic> m_tactic;

    // configuration options
    bool     m_single_pass;
    bool     m_bottom_up;
    bool     m_beta_eta;
    bool     m_iota;
    bool     m_memoize;
    unsigned m_max_steps;
    bool     m_trace;
    bool     m_assumptions;

    void set_options(options const & o) {
        m_single_pass = get_simp_single_pass(o);
        m_bottom_up   = get_simp_bottom_up(o);
        m_beta_eta    = get_simp_beta_eta(o);
        m_iota        = get_simp_iota(o);
        m_memoize     = get_simp_memoize(o);
        m_max_steps   = get_simp_max_steps(o);
        m_trace       = get_simp_trace(o);
        m_assumptions = get_simp_assumptions(o);
    }

public:
    simp_tactic_fn(environment const & env, io_state const & ios, name_generator && ngen,
                   buffer<expr> const & /* ls */, buffer<name> const & /* ns */, buffer<name> const & /* ex */,
                   optional<tactic> const & tac):m_env(env), m_ios(ios), m_ngen(ngen), m_tactic(tac) {
        set_options(m_ios.get_options());
    }

    pair<goal, substitution> operator()(goal const & g, location const & /* loc */, substitution const & s) {
        // TODO(Leo)
        return mk_pair(g, s);
    }
};

tactic mk_simp_tactic(elaborate_fn const & elab, buffer<expr> const & ls, buffer<name> const & ns,
                      buffer<name> const & ex, optional<tactic> tac, location const & loc) {
    return tactic([=](environment const & env, io_state const & ios, proof_state const & s) {
            goals const & gs = s.get_goals();
            if (empty(gs)) {
                throw_no_goal_if_enabled(s);
                return proof_state_seq();
            }
            goal const & g = head(gs);
            name_generator new_ngen  = s.get_ngen();
            substitution   new_subst = s.get_subst();
            buffer<expr>   new_ls;
            for (expr const & l : ls) {
                expr new_l; constraints cs;
                bool report_unassigned = true;
                std::tie(new_l, new_subst, cs) = elab(g, new_ngen.mk_child(), l, none_expr(), new_subst, report_unassigned);
                if (cs)
                    throw_tactic_exception_if_enabled(s, "invalid 'simp' tactic, fail to resolve generated constraints");
                new_ls.push_back(new_l);
            }
            simp_tactic_fn simp(env, ios, new_ngen.mk_child(), new_ls, ns, ex, tac);
            pair<goal, substitution> r = simp(g, loc, new_subst);
            goal new_g = r.first;
            new_subst  = r.second;
            proof_state new_s(s, cons(new_g, tail(gs)), new_subst, new_ngen);

            bool fail_if_metavars = true;
            tactic post_tac = try_tactic(refl_tactic(elab, fail_if_metavars));
            // TODO(Leo): remove now_tactic
            post_tac = then(post_tac, now_tactic());
            return post_tac(env, ios, new_s);
        });
}

void initialize_simp_tactic() {
    name simp_name{"tactic", "simp_tac"};
    g_simp_tactic = new expr(mk_constant(simp_name));

    register_tac(simp_name,
                 [](type_checker & tc, elaborate_fn const & elab, expr const & e, pos_info_provider const * p) {
                     buffer<expr> args;
                     get_app_args(e, args);
                     if (args.size() != 5)
                         throw expr_to_tactic_exception(e, "invalid 'simp' tactic, incorrect number of arguments");
                     buffer<expr> lemmas;
                     get_tactic_expr_list_elements(args[0], lemmas, "invalid 'simp' tactic, invalid argument #1");
                     buffer<name> ns, ex;
                     get_tactic_id_list_elements(args[1], ns, "invalid 'simp' tactic, invalid argument #2");
                     get_tactic_id_list_elements(args[2], ex, "invalid 'simp' tactic, invalid argument #3");
                     optional<tactic> tac;
                     expr A, t;
                     if (is_some(args[3], A, t)) {
                         tac = expr_to_tactic(tc, elab, t, p);
                     } else if (is_none(args[3], A)) {
                         // do nothing
                     } else {
                         throw expr_to_tactic_exception(e, "invalid 'simp' tactic, invalid argument #4");
                     }
                     check_tactic_expr(args[4], "invalid 'simp' tactic, invalid argument #5");
                     expr loc_expr = get_tactic_expr_expr(args[4]);
                     if (!is_location_expr(loc_expr))
                         throw expr_to_tactic_exception(e, "invalid 'simp' tactic, invalid argument #5");
                     location loc = get_location_expr_location(loc_expr);
                     return mk_simp_tactic(elab, lemmas, ns, ex, tac, loc);
                 });


    g_simp_single_pass = new name{"simp", "single_pass"};
    register_bool_option(*g_simp_single_pass, LEAN_DEFAULT_SIMP_SINGLE_PASS,
                         "(simp tactic) if false then the simplifier keeps applying simplifications as long as possible");

    g_simp_bottom_up = new name{"simp", "bottom_up"};
    register_bool_option(*g_simp_bottom_up, LEAN_DEFAULT_SIMP_BOTTOM_UP,
                         "(simp tactic) if true the simplifier uses a bottom up rewriting strategy, otherwise it uses top down");

    g_simp_beta_eta = new name{"simp", "beta_eta"};
    register_bool_option(*g_simp_beta_eta, LEAN_DEFAULT_SIMP_BETA_ETA,
                         "(simp tactic) if true the simplifier applies beta and eta reduction");

    g_simp_iota = new name{"simp", "iota"};
    register_bool_option(*g_simp_iota, LEAN_DEFAULT_SIMP_IOTA,
                         "(simp tactic) if true the simplifier applies iota reduction");

    g_simp_memoize = new name{"simp", "memoize"};
    register_bool_option(*g_simp_memoize, LEAN_DEFAULT_SIMP_MEMOIZE,
                         "(simp tactic) if true the simplifier caches intermediate results");

    g_simp_max_steps = new name{"simp", "max_steps"};
    register_unsigned_option(*g_simp_max_steps, LEAN_DEFAULT_SIMP_MAX_STEPS,
                             "(simp tactic) maximum number of steps that can be performed by the simplifier");

    g_simp_trace = new name{"simp", "trace"};
    register_bool_option(*g_simp_trace, LEAN_DEFAULT_SIMP_TRACE,
                         "(simp tactic) if true the simplifier produces an execution trace for debugging purposes");

    g_simp_assumptions = new name{"simp", "assumptions"};
    register_bool_option(*g_simp_assumptions, LEAN_DEFAULT_SIMP_ASSUMPTIONS,
                         "(simp tactic) if true assumptions/hypotheses are automatically used as rewriting rules");
}

void finalize_simp_tactic() {
    delete g_simp_tactic;
    delete g_simp_single_pass;
    delete g_simp_bottom_up;
    delete g_simp_beta_eta;
    delete g_simp_iota;
    delete g_simp_memoize;
    delete g_simp_max_steps;
    delete g_simp_trace;
    delete g_simp_assumptions;
}
}
