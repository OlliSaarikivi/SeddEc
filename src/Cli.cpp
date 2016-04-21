#include "Pch.hpp"
#include "tclap/CmdLine.h"
#include "Version.hpp"
#include "SeddEcException.hpp"
#include "Z3Utils.hpp"
#include "ESParser.hpp"

#include "cliquer.h"

using ExprSet = unordered_set<expr, Z3Hash, Z3Eq>;

static const double OptimizeTimeout = 10;

class Timer {
    using Clock = std::chrono::steady_clock;
    using Second = std::chrono::duration<double, std::ratio<1>>;
    std::chrono::time_point<Clock> start;
public:
    Timer() : start(Clock::now()) {}
    void reset() { start = Clock::now(); }
    double elapsed() {
        return std::chrono::duration_cast<Second>(Clock::now() - start).count();
    }
};

Timer totalTimer;

string GetETA(double fractionDone) {
    if (fractionDone > 0) {
        double current = totalTimer.elapsed();
        return FORMAT("ETA: " << (current / fractionDone - current) << "s");
    } else
        return "ETA: infinity";
}

void PrintTotalTime() {
    cout << "Total time: " << totalTimer.elapsed() << "s\n";
}

ExprSet GetEventVars(expr term) {
    ExprSet vars;
    ExprSet seen;
    queue<expr> work;
    work.push(term);

    while (!work.empty()) {
        expr current = work.front();
        work.pop();
        if (seen.find(current) != end(seen))
            continue;
        seen.insert(current);

        if (current.is_const()) {
            symbol name = current.decl().name();
            if (name.kind() == Z3_STRING_SYMBOL && name.str().find("el") == 0)
                vars.insert(current);
        }
        else if (current.is_app()) {
            unsigned num = current.num_args();
            for (unsigned i = 0; i < num; i++) {
                work.push(current.arg(i));
            }
        }
        else if (current.is_quantifier()) {
            work.push(current.body());
        }
        else {
            cerr << "unsupported: " << current << "\n";
            exit(1);
        }
    }

    return vars;
}

vector<vector<expr>> Optimize(context& ctx, expr encoding, ExprSet eventVars) {
    vector<vector<expr>> tests;

    unsigned timeout = 2000;

    ExprSet toCover = eventVars;

    int percent = 0;

    auto status = [&]() {
        auto total = eventVars.size();
        auto missing = toCover.size();

        int newPercent = (total - missing) * 100.0 / total;
        if (newPercent != percent) {
            cout << "Tests: " << tests.size() << "\tCovered: " << (total - missing) << "/" << total << "\tProgress: " << newPercent << "%\t";
            cout << GetETA(newPercent / 100.0) << std::endl;
        }
        percent = newPercent;
    };

    auto addTest = [&](const model& model) {
        vector<expr> test;
        for (auto var : eventVars)
            if (eq(model.eval(var), ctx.bool_val(true)))
                test.push_back(var);
        size_t covered = 0;
        for (auto var : test)
            covered += toCover.erase(var);

        if (covered > 0)
            tests.emplace_back(std::move(test));
        return covered != 0;
    };

    auto countCovered = [&](const model& model) {
        unsigned count = 0;
        for (auto var : toCover)
            if (eq(model.eval(var), ctx.bool_val(true)))
                ++count;
        return count;
    };
    
    status();

    unsigned optTimeout = 0; --optTimeout;
    optimize opt{ ctx };
    opt.add(encoding);
    params p{ ctx };
    while (!toCover.empty()) {
        opt.push();

        p.set(":timeout", optTimeout);
        opt.set(p);

        bool first = true;
        for (auto var : toCover) {
            if (first) {
                opt.add(var);
                first = false;
            }
            else
                opt.add(var, 1);
        }

        try {
            auto result = opt.check();
            if (result == unsat) {
                cerr << "Uncoverable event!?\n";
                exit(1);
            } else if (result != sat) {
                cerr << "==Unknown==\n";
                optTimeout = optTimeout * 3 / 2;
            }
            addTest(opt.get_model());
        } catch (z3::exception e) {
            if (e.msg() == string("canceled")) {
                cout << "==Timeout==\n";
                try {
                    auto model = opt.get_model();
                    if (!addTest(model))
                        optTimeout = optTimeout * 3 / 2;
                } catch (z3::exception) {
                    cerr << "Timout without solution!\n";
                    optTimeout = optTimeout * 3 / 2;
                }
            } else throw e;
        }

        status();
        opt.pop();
    }

    return tests;
}

graph_t* ParseCograph(string path) {
    std::ifstream file(path);

    unordered_map<int, int> events;

    string line;
    std::getline(file, line);
    std::istringstream nodes(line);
    int id;
    int n = 0;
    while (nodes >> id) {
        events.emplace(id, n++);
    }

    auto graph = graph_new(n);

    for(;std::getline(file, line);)
    {
        std::istringstream edge(line);
        int left, right;
        if (!(edge >> left >> right))
            throw SeddEcException(Reason::INVALID_INPUT_FORMAT);
        GRAPH_ADD_EDGE(graph, events.at(left), events.at(right));
    }

    graph_test(graph, stdout);
    cout << std::endl;

    return graph;
}

boolean noTime(int,int,int,int,double,double,clique_options *) { return true; }

unsigned CoverCograph(graph_t* graph) {
    unsigned tests = 0;

    clique_options options = *clique_default_options;
    options.time_function = noTime;

    int covered = 0;
    int percent = 0;

    set_t isCovered = set_new(graph->n);
    set_t emptySet = set_new(graph->n);

ADD_TEST:
    set_t maxClique = clique_unweighted_find_single(graph, 0, 0, false, &options);

    if (set_size(maxClique) > 2)
        ++tests;
    else if (set_size(maxClique) == 2) {
        int j = graph->n - 1;
        for (;;) {
            for (; j >= 0 && set_size(graph->edges[j]) == 0; --j) {}
            if (j >= 0) {
                ++tests;
                set_t clique = set_new(graph->n);
                SET_ADD_ELEMENT(clique, j);
                SET_ADD_ELEMENT(clique, set_return_next(graph->edges[j], -1));

                set_t oldIsCovered = set_duplicate(isCovered);
                set_union(isCovered, oldIsCovered, clique);
                set_free(oldIsCovered);
                for (int k = 0; k < j; ++k) {
                    set_remove(graph->edges[k], clique);
                }
                int i=-1;
                while ((i=set_return_next(clique,i))>=0) {
                    set_union(graph->edges[i], emptySet, emptySet);
                }
                set_free(clique);
            } else {
                tests += graph->n - set_size(isCovered);
                set_free(isCovered);
                set_free(emptySet);
                return tests;
            }
            
            int newPercent = set_size(isCovered) * 100.0 / graph->n;
            if (newPercent != percent) {
                cout << "Covered: " << set_size(isCovered) << "/" << graph->n << "\tProgress: " << newPercent << "%\t";
                cout << GetETA(newPercent / 100.0) << std::endl;
            }
            percent = newPercent;
        }
    }
    else {
        tests += graph->n - set_size(isCovered);
        set_free(isCovered);
        set_free(emptySet);
        return tests;
    }

    set_t oldIsCovered = set_duplicate(isCovered);
    set_union(isCovered, oldIsCovered, maxClique);
    set_free(oldIsCovered);
    for (int j = 0; j < graph->n; ++j) {
        set_remove(graph->edges[j], maxClique);
    }
    int i=-1;
    while ((i=set_return_next(maxClique,i))>=0) {
        set_union(graph->edges[i], emptySet, emptySet);
//        if (graph->weights[i] != 0) {
//            graph->weights[i] = 0;
//            ++covered;
//        }
    }

    int newPercent = set_size(isCovered) * 100.0 / graph->n;
    if (newPercent != percent) {
        cout << "Covered: " << set_size(isCovered) << "/" << graph->n << "\tProgress: " << newPercent << "%\t";
        cout << GetETA(newPercent / 100.0) << std::endl;
    }
    percent = newPercent;
    set_free(maxClique);
//    for (int j = 0; j < graph->n; ++j) {
//        if (graph->weights[j] != 0) goto ADD_TEST;
//    }
    if (set_size(isCovered) < graph->n) goto ADD_TEST;

    set_free(isCovered);
    set_free(emptySet);
    return tests;
}

expr EncodeEvents(context& ctx, const vector<Event>& events) {
    unordered_set<unsigned> leafEvents;
    for (auto& event : events)
        leafEvents.emplace(event.id);
    for (auto& event : events)
        for (auto& pred : event.predecessors)
            leafEvents.erase(pred);

    auto eventVar = [&](unsigned id) {
        return ctx.bool_const(FORMAT((leafEvents.find(id) == leafEvents.end() ? "e" : "el") << id).c_str());
    };

    expr encoding = ctx.bool_val(true);
    for (auto& event : events) {
        auto var = eventVar(event.id);
        encoding = encoding && (var || !var);
        for (auto& pred : event.predecessors)
            encoding = encoding && implies(var, eventVar(pred));
        expr conflictSet = ctx.bool_val(false);
        for (auto& conflict : event.conflicts)
            conflictSet = conflictSet || eventVar(conflict);
        encoding = encoding && implies(var, !conflictSet);
    }

    return encoding;
}

graph_t* SolveCograph(context& ctx, expr encoding, ExprSet eventVars) {
    auto graph = graph_new(eventVars.size());

    vector<expr> vars;
    for (auto var : eventVars)
        vars.emplace_back(var);

    solver s{ ctx };
    s.add(encoding);

    int percent = 0;

    while (!vars.empty()) {
        auto outerVar = vars.back();
        vars.pop_back();

        int current = vars.size();

        s.push();
        s.add(outerVar);
        for (int i = 0; i < vars.size(); ++i) {
            expr_vector assumptions{ ctx };
            assumptions.push_back(vars[i]);
            if (s.check(assumptions) == sat)
                GRAPH_ADD_EDGE(graph, current, i);
        }
        s.pop();

        int newPercent = (eventVars.size() - current) * 100.0 / eventVars.size();
        if (newPercent != percent)
            cout << newPercent << "%\n";
        percent = newPercent;
    }

    graph_test(graph, stdout);
    return graph;
}

int main(int argc, char** argv) {
    totalTimer.reset();

    std::stringstream version;
    version << VERSION_MAJOR_FROM_CMAKE << '.' << VERSION_MINOR_FROM_CMAKE;

    try {
        TCLAP::CmdLine cmd("", ' ', version.str());

        TCLAP::ValueArg<string> smt2Path("s", "smt2", "SMT2 input file", false, "", "path");
        cmd.add(smt2Path);
        TCLAP::ValueArg<string> cographPath("g", "cograph", "Cograph input file (use Cliquer)", false, "", "path");
        cmd.add(cographPath);
        TCLAP::ValueArg<string> esPath("e", "event-structure", "Event structure input file", false, "", "path");
        cmd.add(esPath);

        TCLAP::SwitchArg useCliquer("c", "cliquer", "Solve a cograph and find tests with Cliquer");
        cmd.add(useCliquer);
		
        cmd.parse(argc, argv);

        if (smt2Path.getValue() != "" || esPath.getValue() != "") {
            context ctx;
            expr encoding{ ctx };
            if (smt2Path.getValue() != "") {
                cout << "INPUT: " << smt2Path.getValue() << std::endl;
                encoding = to_expr(ctx, Z3_parse_smtlib2_file(ctx, smt2Path.getValue().c_str(), 0, nullptr, nullptr, 0, nullptr, nullptr));
            }
            else {
                cout << "INPUT: " << esPath.getValue() << std::endl;
                auto events = ParseEventStructure(esPath.getValue());
                encoding = EncodeEvents(ctx, events);
            }
            auto eventVars = GetEventVars(encoding);
            if (useCliquer.getValue()) {
                cout << "METHOD: Cograph from Z3 + Cliquer\n";
                auto cograph = SolveCograph(ctx, encoding, eventVars);
                auto numTests = CoverCograph(cograph);
                graph_free(cograph);
                cout << "Tests in cover (Cliquer): " << numTests << std::endl;
            } else {
                cout << "METHOD: Z3\n";
                auto tests = Optimize(ctx, encoding, eventVars);
                cout << "Tests in cover (Z3): " << tests.size() << std::endl;
            }
        }
        else if (cographPath.getValue() != "") {
            cout << "INPUT: " << cographPath.getValue() << std::endl;
            cout << "METHOD: Cliquer\n";
            auto cograph = ParseCograph(cographPath.getValue());
            auto numTests = CoverCograph(cograph);
            graph_free(cograph);
            cout << "Tests in cover (Cliquer): " << numTests << std::endl;
        }
        else {
            cerr << "Please provide an input path\n";
            return 1;
        }
    } catch (TCLAP::ArgException& e) {
        cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        return 2;
	}
	catch (SeddEcException& e) {
		cerr << e.what() << std::endl;
		return 3;
	}

    PrintTotalTime();
}