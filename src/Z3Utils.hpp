#pragma once

inline bool IsValid(expr term) {
    solver s{term.ctx()};
    s.add(!term);
    return s.check() == unsat;
}

inline bool AreEquivalent(expr left, expr right, expr assumption) {
    return eq(left, right) || IsValid(implies(assumption, left == right));
}

inline bool IsSatisfiable(expr term) {
    solver s{term.ctx()};
    s.add(term);
    return s.check() == sat;
}

template<class T>
ast_vector_tpl<T> ToZ3Vec(const vector<T> from, context& ctx) {
    ast_vector_tpl<T> to{ctx};
    for (auto& x : from)
        to.push_back(x);
    return to;
}

struct Z3Hash {
    template<class T>
    size_t operator()(const T& x) const {
        return (size_t)(x.hash());
    }
};

struct Z3Eq {
    template<class T, class U>
    bool operator()(const T& x, const U& y) const {
        return eq(x, y);
    }
};

inline expr MkAtMost(expr_vector vars, unsigned k) {
    array<Z3_ast> _vars(vars.size());
    for (unsigned i = 0; i < vars.size(); ++i) {
        _vars[i] = vars[i];
    }
	expr r{ vars.ctx(), Z3_mk_atmost(vars.ctx(), vars.size(), _vars.ptr(), k) };
	vars.check_error();
	return r;
}