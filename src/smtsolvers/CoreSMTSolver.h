/*********************************************************************
Author: Antti Hyvarinen <antti.hyvarinen@gmail.com>

OpenSMT2 -- Copyright (C) 2012 - 2015 Antti Hyvarinen
                         2008 - 2012 Roberto Bruttomesso

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/

/****************************************************************************************[Solver.C]
MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef MINISATSMTSOLVER_H
#define MINISATSMTSOLVER_H

#define CACHE_POLARITY     0

#include "SMTSolver.h"

#include <cstdio>
#include <arpa/inet.h>

#include "Vec.h"
#include "Heap.h"
#include "Alg.h"

#include "SolverTypes.h"
//#include "LA.h"

#ifdef PRODUCE_PROOF
#include "ProofGraph.h"
#include "Proof.h"
class Proof;
#endif

//---------------------------------------------------------------------------------------
// State for CNFs and mappings for terms
//
//
// Struct for communicating the cnf and the mapping between variables and PTRefs
//

struct VarPtPair {
    Var v;
    PTRef tr;
};

class CnfState {
    char*               cnf;
    vec<VarPtPair>      map;
    bool                unsat;
public:
    CnfState() : cnf(NULL), unsat(false) {};
    ~CnfState() { free(cnf); }
    void                  setUnsat()            { assert(cnf == NULL); unsat = true; }
    const char*           getCnf()              { return unsat ? "1 -1 0" : (cnf == NULL ? "c empty" : cnf); }
    void                  setCnf(char* cnf_)    { cnf = cnf_; }
    const vec<VarPtPair>& getMap()              { return map; }
    void                  addToMap(VarPtPair p) { map.push(p); }
};

// -----------------------------------------------------------------------------------------
// The splits
//
class SplitData
{
    vec<Clause*>&       inst_clauses;   // Reference to the instance clause database
    vec<Lit>&           trail;          // Solver's trail for level 0 unit clauses
    int                 trail_idx;      // First index not on level 0 at the time of insertion

    vec<vec<Lit> >      constraints;    // The split constraints
    vec<vec<Lit> >      learnts;        // The learnt clauses
    vec<vec<Lit> >      instance;       // The copy of the instance as it was when updateInstance was called

    char* litToString(const Lit);
    template<class C> char* clauseToString(const C&);
    char* clauseToString(const vec<Lit>&);
    int getLitSize(const Lit l) const;

  public:
    SplitData(vec<Clause*>& ic, vec<Lit>& t, int tl)
        : inst_clauses(ic)
        , trail(t)
        , trail_idx(tl)
    {}
    SplitData(const SplitData& other)
        : inst_clauses(other.inst_clauses)
        , trail(other.trail)
        , trail_idx(other.trail_idx)
    { assert(other.instance.size() == 0 && other.constraints.size() == 0 && other.learnts.size() == 0); }

    template<class C> void addConstraint(const C& c) {
        constraints.push();
        vec<Lit>& cstr = constraints.last();
        for (int i = 0; i < c.size(); i++)
            cstr.push(c[i]); }
    void addLearnt(Clause& c) {
        learnts.push();
        vec<Lit>& learnt = learnts.last();
        for (int i = 0; i < c.size(); i++)
            learnt.push(c[i]); }
    void updateInstance() {
        assert(instance.size() == 0);
        for (int i = 0; i < inst_clauses.size(); i++) {
            instance.push();
            vec<Lit>& c_o = instance.last();
            Clause& c_i = *(inst_clauses[i]);
            for (int j = 0; j < c_i.size(); j++)
                c_o.push(c_i[j]);
        }
    }
    char* splitToString();
    void  cnfToString(CnfState& cs) { cs.setCnf(splitToString()); }
};

inline int SplitData::getLitSize(const Lit l) const
{
    int sz = 0;
    if (sign(l)) sz++;
    Var v = var(l);
    int n = v+1;
    while (n != 0) { n /= 10; sz++; }
    return sz;
}

inline char* SplitData::litToString(const Lit l)
{
    char* l_str;
    asprintf(&l_str, "%s%d", sign(l) ? "-" : "", var(l)+1);
    return l_str;
}

template<class C>
inline char* SplitData::clauseToString(const C& c)
{
    char* c_str = (char*)malloc(1);
    c_str[0] = 0;
    char* c_old = c_str;
    for (int i = 0; i < c.size(); i++) {
        char* l_str = litToString(c[i]);
        c_old = c_str;
        asprintf(&c_str, "%s%s ", c_old, l_str);
        free(l_str);
        free(c_old);
    }
    c_old = c_str;
    asprintf(&c_str, "%s0", c_str);
    free(c_old);
    return c_str;
}

inline char* SplitData::splitToString()
{
    int buf_cap = 1024;
    int sz = 0;
    char* buf = (char*) malloc(1024);


    // Units in dl 0
    for (int i = 0; i < (trail_idx > 0 ? trail_idx : trail.size()); i++) {
        int n = getLitSize(trail[i]);
        while (buf_cap < sz + n + 4) { // The size of lit, trailing space, the zero, the newline, and NULL
            buf_cap *= 2;
            buf = (char*) realloc(buf, buf_cap);
        }
        sprintf(&buf[sz], "%s%d 0\n", sign(trail[i]) ? "-" : "", var(trail[i])+1);
        sz += n+3; // points to NULL
    }

    // The constraints
    for (int i = 0; i < constraints.size(); i++) {
        vec<Lit>& c = constraints[i];
        for (int j = 0; j < c.size(); j++) {
            Lit l = c[j];
            int n = getLitSize(l);
            while (buf_cap < sz + n + 2) { // Lit, space, and NULL
                buf_cap *= 2;
                buf = (char*) realloc(buf, buf_cap);
            }
            sprintf(&buf[sz], "%s%d ", sign(l) ? "-" : "", var(l)+1);
            sz += n+1; // Points to the NULL
        }
        while (buf_cap < sz + 3) { // 0, newline, and NULL
            buf_cap *= 2;
            buf = (char*) realloc(buf, buf_cap);
        }
        sprintf(&buf[sz], "0\n");
        sz += 2;
    }

    // The instance
    for (int i = 0; i < instance.size(); i++) {
        vec<Lit> &c = instance[i];
        for (int j = 0; j < c.size(); j++) {
            Lit l = c[j];
            int n = getLitSize(l);
            while (buf_cap < sz + n + 2) { // The size of lit, the trailing space, and NULL
                buf_cap *= 2;
                buf = (char*) realloc(buf, buf_cap);
            }
            sprintf(&buf[sz], "%s%d ", sign(l) ? "-" : "", var(l)+1);
            sz += n+1; // points to the NULL
        }
        while (buf_cap < sz + 3) { // zero, newline and NULL
            buf_cap *= 2;
            buf = (char*) realloc(buf, buf_cap);
        }
        sprintf(&buf[sz], "0\n");
        sz += 2; // points to the NULL
    }
    for (int i = 0; i < learnts.size(); i++) {
        vec<Lit>& c = learnts[i];
        for (int j = 0; j < c.size(); j++) {
            Lit l = c[j];
            int n = getLitSize(l);
            while (buf_cap < sz + n + 2) { // The size of lit, space, and NULL
                buf_cap *= 2;
                buf = (char*) realloc(buf, buf_cap);
            }
            sprintf(&buf[sz], "%s%d ", sign(l) ? "-" : "", var(l)+1);
            sz += n+1; // points to the null
        }
        while (buf_cap < sz + 3) { // The zero, newline, and the NULL
            buf_cap *= 2;
            buf = (char*) realloc(buf, buf_cap);
        }
        sprintf(&buf[sz], "0\n");
        sz += 2; // Points to the NULL
    }
    buf = (char*) realloc(buf, sz+1);
    return buf;
}

class LANode {
  public:
    // c1 & c2 are for debugging
    LANode* c1; LANode* c2; LANode* p; Lit l; lbool v; int d;
    LANode() : c1(NULL), c2(NULL), p(NULL), l(lit_Undef), v(l_Undef), d(0) {}
    LANode(LANode* par, Lit li, lbool va, int dl) :
        c1(NULL), c2(NULL), p(par), l(li), v(va), d(dl) {}
    void print() {
        for (int i = 0; i < d; i++)
            printf(" ");
        printf("%s%d [%s, %d]", sign(l) ? "-" : "", var(l), v == l_False ? "unsat" : "open", d);

        if (c1 != NULL) {
            printf(" c1");
        }
        if (c2 != NULL) {
            printf(" c2");
        }
        printf("\n");
        if (c1 != NULL)
            c1->print();
        if (c2 != NULL)
            c2->print();
    }
};


//=================================================================================================
// Solver -- the main class:

class CoreSMTSolver : public SMTSolver
{
  public:

    // Constructor/Destructor:
    //
    CoreSMTSolver(SMTConfig&, THandler&);
    ~CoreSMTSolver();
    void     initialize       ( );
/*
*/

    // Problem specification:
    //
    Var     newVar    (bool polarity = true, bool dvar = true); // Add a new variable with parameters specifying variable mode.
    bool    addClause ( const vec<Lit> & ps
#ifdef PRODUCE_PROOF
	              , const ipartitions_t& = 1 
#endif
		      ,bool shared=false);                                        // Add a clause to the solver. NOTE! 'ps' may be shrunk by this method!
    // Solving:
    //
    bool    simplify     ();                        // Removes already satisfied clauses.
    void    declareVarsToTheories();                 // Declare the seen variables to the theories
    lbool   solve        ( const vec< Lit > & assumps );                 // Search for a model that respects a given set of assumptions.
    lbool   solve        ( const vec< Lit > & assumps, const unsigned ); // Search for a model that respects a given set of assumptions.
    lbool   solve        ();                        // Search without assumptions.


    void    crashTest    (int, Var, Var);           // Stress test the theory solver
    virtual bool  okay   () const;                  // FALSE means solver is in a conflicting state

    // Variable mode:
    //
    void    setPolarity    (Var v, bool b); // Declare which polarity the decision heuristic should use for a variable. Requires mode 'polarity_user'.
    void    setDecisionVar (Var v, bool b); // Declare if a variable should be eligible for selection in the decision heuristic.
    // Read state:
    //
    lbool   value      (Var x) const;       // The current value of a variable.
    lbool   value      (Lit p) const;       // The current value of a literal.
    lbool   modelValue (Lit p) const;       // The value of a literal in the last model. The last call to solve must have been satisfiable.
    int     nAssigns   ()      const;       // The current number of assigned literals.
    int     nClauses   ()      const;       // The current number of original clauses.
    int     nLearnts   ()      const;       // The current number of learnt clauses.
    int     nVars      ()      const;       // The current number of variables.

    //=================================================================================================
    // Added Code
/*
    void addSMTAxiomClause  ( vector< Enode * > & 
#ifdef PRODUCE_PROOF
	                    , Enode * 
#endif
	                    );

#ifdef PRODUCE_PROOF
    Enode * computeAxiomInterp ( vector< Enode * > & );
#endif

    void addNewAtom         ( Enode * );
*/
    vec< Clause * >          axioms;         // List of axioms produced with splitting on demand
    int                      axioms_checked; // Id of next axiom to be checked

#ifdef PRODUCE_PROOF
    set< int >               axioms_ids;     // Set of ids for lemmas on demand
#endif

    // External support incremental and backtrackable APIs
    void        pushBacktrackPoint ( );
    void        popBacktrackPoint  ( );
    void        reset              ( );
    inline void restoreOK          ( )       { ok = true; }
    inline bool isOK               ( ) const { return ok; }

/*
    template<class C>
    void     printSMTClause   ( ostream &, const C& );
    void     printSMTClause   ( ostream &, vec< Lit > &, bool = false );
    void     printSMTClause   ( ostream &, vector< Lit > &, bool = false );
*/
    set< Clause * > detached;

    // Added Code
    //=================================================================================================

    // Extra results: (read-only member variable)
    //
//    vec<lbool> model;             // If problem is satisfiable, this vector contains the model (if any). (moded to SMTSolver)
    vec<Lit>   conflict;          // If problem is unsatisfiable (possibly under assumptions),
    // this vector represent the final conflict clause expressed in the assumptions.

    // Mode of operation:
    //
    double    var_decay;          // Inverse of the variable activity decay factor.                                            (default 1 / 0.95)
    double    clause_decay;       // Inverse of the clause activity decay factor.                                              (1 / 0.999)
    double    random_var_freq;    // The frequency with which the decision heuristic tries to choose a random variable.        (default 0.02)
    int       restart_first;      // The initial restart limit.                                                                (default 100)
    double    restart_inc;        // The factor with which the restart limit is multiplied in each restart.                    (default 1.1)
    double    learntsize_factor;  // The intitial limit for learnt clauses is a factor of the original clauses.                (default 1 / 3)
    double    learntsize_inc;     // The limit for learnt clauses is multiplied with this factor each restart.                 (default 1.1)
    bool      expensive_ccmin;    // Controls conflict clause minimization.                                                    (default TRUE)
    int       polarity_mode;      // Controls which polarity the decision heuristic chooses. See enum below for allowed modes. (default polarity_false)

    enum { polarity_true = 0, polarity_false = 1, polarity_user = 2, polarity_rnd = 3 };



    // Statistics: (read-only member variable)
    //
    uint64_t starts, decisions, rnd_decisions, propagations, conflicts, conflicts_last_update;
    uint64_t clauses_literals, learnts_literals, max_literals, tot_literals;
    double learnts_size;
    uint64_t all_learnts;
    uint64_t learnt_theory_conflicts;
    uint64_t top_level_lits;

    // Splits
    vec<SplitData> splits;

  protected:

    // Helper structures:
    //
    struct VarOrderLt {
      const vec<double>&  activity;
      bool operator () (Var x, Var y) const { return activity[x] > activity[y]; }
      VarOrderLt(const vec<double>&  act) : activity(act) { }
    };

    friend class VarFilter;
    struct VarFilter {
      const CoreSMTSolver& s;
      VarFilter(const CoreSMTSolver& _s) : s(_s) {}
      bool operator()(Var v) const { return toLbool(s.assigns[v]) == l_Undef && s.decision_var[v]; }
    };

    // Solver state:
    //
    bool                ok;               // If FALSE, the constraints are already unsatisfiable. No part of the solver state may be used!
    uint32_t n_clauses;             // number of clauses in the problem
    vec<Clause*>        clauses;          // List of problem clauses.
    vec<Clause*>        learnts;          // List of learnt clauses.
    vec<Clause*>        tmp_reas;         // Reasons for minimize_conflicts 2
#ifdef PEDANTIC_DEBUG
    vec<Clause*>        debug_reasons;    // Reasons for the theory deduced clauses
    Map<Var,int,VarHash> debug_reason_map; // Maps the deduced lit to the clause used to deduce it
#endif
    double              cla_inc;          // Amount to bump next clause with.
    vec<double>         activity;         // A heuristic measurement of the activity of a variable.
    double              var_inc;          // Amount to bump next variable with.
    vec<vec<Clause*> >  watches;          // 'watches[lit]' is a list of constraints watching 'lit' (will go there if literal becomes true).
    vec<char>           assigns;          // The current assignments (lbool:s stored as char:s).
    vec<bool>           var_seen;

    // Stuff specific to the lookahead implementation

    uint32_t latest_round;                      // The numbering for arrays
    void updateRound() { latest_round++; }
    // -----------------------------------------------------------------------------------------
    // Data type for exact value array
    static inline int min(int i, int j) { return i < j ? i : j; }
    static inline int max(int i, int j) { return i > j ? i : j; }

    class ExVal {
      private:
         int pprops;
         int nprops;
         int round;
      public:
         ExVal() : pprops(-1), nprops(-1), round(-1) {}
         ExVal(int p, int n, int r) : pprops(p), nprops(n), round(r) {}
         bool operator< (const ExVal& e) const {
             return (round < e.round) ||
                 (min(pprops, nprops) < min(e.pprops, e.nprops)) ||
                 ((min(pprops, nprops) == min(e.pprops, e.nprops)) && (max(pprops, nprops) < max(e.pprops, e.nprops))); }
         bool betterPolarity() const { return pprops < nprops; } // Should return false if the literal should be unsigned
         int  getRound() const { return round; }
         int  getEx_l() const  { return min(pprops, nprops); }
         int  getEx_h() const  { return max(pprops, nprops); }
         void setRound(int r)  { round = r; }
    };

    // -----------------------------------------------------------------------------------------
    // Data type for upper bound array
    //
    class UBel {
      public:
        int ub;
        int round;

        UBel() : ub(-1), round(-1) {}
        UBel(int u, int r) : ub(u), round(r) {}

        bool operator== (const UBel& o) const { return ub == o.ub && round == o.round; }
        bool operator!= (const UBel& o) const { return !(operator==(o)); }
    };

    static const CoreSMTSolver::UBel UBel_Undef;

    class UBVal {
      private:
        UBel ub_p;
        UBel ub_n;
        bool current(const ExVal& e) const { return ub_p.round == ub_n.round && ub_p.round == e.getRound(); }
      public:
        UBVal() {}
        UBVal(const UBel& ub_pos, const UBel& ub_neg) : ub_p(ub_pos), ub_n(ub_neg) {}
        void updateUB_p(const UBel& ub) { if (ub_p.round < ub.round || (ub_p.round == ub.round && ub_p.ub > ub.ub)) ub_p = ub; }
        void updateUB_n(const UBel& ub) { if (ub_n.round < ub.round || (ub_n.round == ub.round && ub_n.ub > ub.ub)) ub_n = ub; }

        bool safeToSkip(const ExVal& e) const;
        const UBel& getUB_p()  const { return ub_p; }
        const UBel& getUB_n()  const { return ub_n; }
        bool betterThan(const ExVal& e) const;
        const UBel& getLow() const { if (ub_p.round != ub_n.round) return UBel_Undef; else return ub_p.ub < ub_n.ub ? ub_p : ub_n; }
        const UBel& getHigh() const {if (ub_p.round != ub_n.round) return UBel_Undef; else return  ub_p.ub < ub_n.ub ? ub_n : ub_p; }
    };

    bool betterThan_ub(const UBVal& ub, const ExVal& e) const;

    void updateLABest(Var v);

    vec<UBVal>          LAupperbounds;    // The current upper bounds
    vec<ExVal>          LAexacts;         // The current exact values
    vec<char>           polarity;         // The preferred polarity of each variable.
    vec<char>           decision_var;     // Declares if a variable is eligible for selection in the decision heuristic.
  public:
    vec<int>            n_occs;           // Number of occurrences of a variable in clauses
  protected:
#ifdef PEDANTIC_DEBUG
  public:
    vec<Lit>            trail;            // Assignment stack; stores all assigments made in the order they were made.
  protected:
#else
    vec<Lit>            trail;            // Assignment stack; stores all assigments made in the order they were made.
#endif
    vec<int>            trail_lim;        // Separator indices for different decision levels in 'trail'.
#ifdef PRODUCE_PROOF
    vec<int>            trail_pos;        // 'trail_pos[var]' is the variable's position in 'trail[]'. This supersedes 'level[]' in some sense, and 'level[]' will probably be removed in future releases.
#endif
    vec<Clause*>        reason;           // 'reason[var]' is the clause that implied the variables current value, or 'NULL' if none.
    vec<int>            level;            // 'level[var]' contains the level at which the assignment was made.
    int                 qhead;            // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
    int                 simpDB_assigns;   // Number of top-level assignments since last execution of 'simplify()'.
    int64_t             simpDB_props;     // Remaining number of propagations that must be made before next execution of 'simplify()'.
    vec<Lit>            assumptions;      // Current set of assumptions provided to solve by the user.
    Heap<VarOrderLt>    order_heap;       // A priority queue of variables ordered with respect to the variable activity.
    double              random_seed;      // Used by the random variable selection.
    double              progress_estimate;// Set by 'search()'.
    bool                remove_satisfied; // Indicates whether possibly inefficient linear scan for satisfied clauses should be performed in 'simplify'.

#ifdef CACHE_POLARITY
    vec<char>           prev_polarity;    // The previous polarity of each variable.
#endif

    int la_round;                         // Keeps track of the lookahead round (used in lower bounds)
    Lit LABestLit;                        // The current best literal of lookahead

    // Temporaries (to reduce allocation overhead). Each variable is prefixed by the method in which it is
    // used, exept 'seen' wich is used in several places.
    //
    vec<char>           seen;
    vec<Lit>            analyze_stack;
    vec<Lit>            analyze_toclear;
    vec<Lit>            add_tmp;

#ifdef PRODUCE_PROOF
    vec<Lit>            analyze_proof;
    vec< Clause * >     units;
#endif

    SpUnit   resource_units;
    double   resource_limit;      // Time limit for the search in resource_units
    double   next_resource_limit; // The limit at which the solver needs to stop

    // splitting state vars
    SpType   split_type;

    bool     split_on;
    bool     split_start;
    int      split_num;
    SpUnit   split_units;

    double   split_inittune;                                                           // Initial tuning in units.
    double   split_midtune;                                                            // mid-tuning in units.
    double   split_next;                                                               // When the next split will happen?

    SpPref   split_preference;


    // Main internal methods:
    //
    void     updateSplitState();                                                       // Update the state of the splitting machine.
    bool     scatterLevel();                                                           // Are we currently on a scatter level.
    bool     createSplit_scatter(bool last);                                           // Create a split formula and place it to the splits vector.
    bool     createSplit_lookahead();                                                  // Does not change the formula
    bool     excludeAssumptions(vec<Lit>& neg_constrs);                                // Add a clause to the database and propagate
    void     insertVarOrder   (Var x);                                                 // Insert a variable in the decision order priority queue.
    Lit      pickBranchLit    (int polarity_mode, double random_var_freq);             // Return the next decision variable.
    void     newDecisionLevel ();                                                      // Begins a new decision level.
    void     uncheckedEnqueue (Lit p, Clause* from = NULL);                            // Enqueue a literal. Assumes value of literal is undefined.
    bool     enqueue          (Lit p, Clause* from = NULL);                            // Test if fact 'p' contradicts current state, enqueue otherwise.
    Clause*  propagate        ();                                                      // Perform unit propagation. Returns possibly conflicting clause.
    void     cancelUntil      (int level);                                             // Backtrack until a certain level.
    void     analyze          (Clause* confl, vec<Lit>& out_learnt, int& out_btlevel); // (bt = backtrack)
    void     analyzeFinal     (Lit p, vec<Lit>& out_conflict);                         // COULD THIS BE IMPLEMENTED BY THE ORDINARIY "analyze" BY SOME REASONABLE GENERALIZATION?
    bool     litRedundant     (Lit p, uint32_t abstract_levels);                       // (helper method for 'analyze()')
    lbool    search           (int nof_conflicts, int nof_learnts);                    // Search for a given number of conflicts.
    void     reduceDB         ();                                                      // Reduce the set of learnt clauses.
    void     removeSatisfied  (vec<Clause*>& cs);                                      // Shrink 'cs' to contain only non-satisfied clauses.

    void     updateLAUB       (Lit l, int props);                                      // Check the lookahead upper bound and update it if necessary
    lbool    lookahead_loop   (Lit& best, int &idx);
    void     setLAExact       (Var v, int pprops, int nprops);                         // Set the exact la value
    Lit      getLABest        () { if (LAexacts[var(LABestLit)].getRound() < latest_round) { return lit_Undef; } else { return LABestLit; } }
    bool     LApropagate_wrapper();

    // Maintaining Variable/Clause activity:
    //
    void     varDecayActivity ();                      // Decay all variables with the specified factor. Implemented by increasing the 'bump' value instead.
    void     varBumpActivity  (Var v);                 // Increase a variable with the current 'bump' value.

    // Added Line
    void     boolVarDecActivity( );                    // Decrease boolean atoms activity
    void     claDecayActivity  ( );                    // Decay all clauses with the specified factor. Implemented by increasing the 'bump' value instead.
    void     claBumpActivity   ( Clause & c );         // Increase a clause with the current 'bump' value.
    void     mixedVarDecActivity( );                   // Increase a clause with the current 'bump' value.


    // Operations on clauses:
    //
    void     attachClause     (Clause& c);             // Attach a clause to watcher lists.
    void     detachClause     (Clause& c);             // Detach a clause to watcher lists.
    void     removeClause     (Clause& c);             // Detach and free a clause.
    bool     locked           (const Clause& c) const; // Returns TRUE if a clause is a reason for some implication in the current state.
    bool     satisfied        (const Clause& c) const; // Returns TRUE if a clause is satisfied in the current state.

    // Misc:
    //
    int      decisionLevel    ()      const; // Gives the current decisionlevel.
    uint32_t abstractLevel    (Var x) const; // Used to represent an abstraction of sets of decision levels.
    double   progressEstimate ()      const; // DELETE THIS ?? IT'S NOT VERY USEFUL ...


// Debug:
//    void     printLit         (Lit l);
//    template<class C>
//    void     printClause      (const C& c);
/*
    void     printSMTLit              ( ostream &, const Lit );
*/
#ifdef PRODUCE_PROOF
    void     printRestrictedSMTClause ( ostream &, vec< Lit > &, const ipartitions_t & );
    Enode *  mkRestrictedSMTClause    ( vec< Lit > &, const ipartitions_t & );
#endif
    void     verifyModel      ();
    void     checkLiteralCount();

    //ADDED FOR MINIMIZATION
    void     printClause      ( vec< Lit > & );

    // Static helpers:
    //

    // Returns a random float 0 <= x < 1. Seed must never be 0.
    static inline double drand(double& seed) {
      seed *= 1389796;
      int q = (int)(seed / 2147483647);
      seed -= (double)q * 2147483647;
      return seed / 2147483647; }

      // Returns a random integer 0 <= x < size. Seed must never be 0.
      static inline int irand(double& seed, int size) {
	return (int)(drand(seed) * size); }


      //=================================================================================================
      // Added Code

  public:

      void     printLit         (Lit l);
      template<class C>
      void     printClause      (const C& c);

      void     cnfToString      (CnfState&);

      lbool    smtSolve         ( );             // Solve
#ifndef SMTCOMP
/*
      lbool  getModel               ( Enode * );
*/
      void   printModel             ( );             // Wrapper
      void   printModel             ( ostream & );   // Prints model
#endif
/*
#ifdef PRODUCE_PROOF
      void   printProof              ( ostream & );
      void   printInter              ( ostream & );   // Print interpolants
      void   GetInterpolants         (const vector<vector<int> >& partitions, vector<Enode*>& interpolants);
      void   getMixedAtoms           ( set< Var > & );
      void   checkPartitions         ( );
      void   verifyInterpolantWithExternalTool ( vector< Enode * > & );
      inline const ipartitions_t & getIPartitions ( Clause * c )            { assert( clause_to_partition.find( c ) != clause_to_partition.end( ) ); return clause_to_partition[ c ]; }
      inline Enode *               getInterpolants( Clause * c )            { assert( clause_to_in.find( c ) != clause_to_in.end( ) ); return clause_to_in[ c ]; }
      inline void                  setInterpolant ( Clause * c, Enode * e ) { clause_to_in[ c ] = e; }
#endif
*/
  protected:

#ifdef STATISTICS
      void   printStatistics        ( ostream & );   // Prints statistics
#endif
      void   printTrail             ( );             // Prints the trail (debugging)
      int    checkTheory            ( bool );        // Checks consistency in theory

      void   deduceTheory           (vec<LitLev>&);  // Perform theory-deductions

      int    checkAxioms            ( );             // Checks consistency of lemma on demand

      int    analyzeUnsatLemma      ( Clause * );    // Conflict analysis for an unsat lemma on demand

      void   cancelUntilVar         ( Var );         // Backtrack until a certain variable
      void   cancelUntilVarTempInit ( Var );         // Backtrack until a certain variable
      void   cancelUntilVarTempDone ( );             // Backtrack until a certain variable
      int    restartNextLimit       ( int );         // Next conflict limit for restart
/*
      Var    generateMoreEij        ( );             // Generate more eij
      Var    generateNextEij        ( );             // Generate next eij
*/
#ifndef SMTCOMP
      void   dumpCNF                ( );             // Dumps CNF to cnf.smt2
/*
      void   dumpRndInter           ( );             // Dumps a random interpolation problem
*/
#endif
      Clause *           fake_clause;                // Fake clause for unprovided reasons
      vec< Clause * >    cleanup;                    // For cleaning up
      bool               first_model_found;          // True if we found a first boolean model
      double             skip_step;                  // Steps to skip in calling tsolvers
      long               skipped_calls;              // Calls skipped so far
      long               learnt_t_lemmata;           // T-Lemmata stored during search
      long               perm_learnt_t_lemmata;      // T-Lemmata stored during search
      unsigned           luby_i;                     // Keep track of luby index
      unsigned           luby_k;                     // Keep track of luby k
      vector< unsigned > luby_previous;              // Previously computed luby numbers
      bool               cuvti;                      // For cancelUntilVarTemp
      vec<Lit>           lit_to_restore;             // For cancelUntilVarTemp
      vec<char>          val_to_restore;             // For cancelUntilVarTemp

#ifdef PRODUCE_PROOF
      //
      // Proof production
      //
      Proof *             proof_;                   // (Pointer to) Proof store
      Proof &             proof;                    // Proof store
      vec< Clause * >     pleaves;                  // Store clauses that are still involved in the proof
      vec< Clause * >     tleaves;                  // Store theory clauses to be removed
      // TODO: Maybe change to vectors
      map< Clause *, PTRef >                  clause_to_in;        // Clause id to interpolant (for theory clauses)
      vector< pair< Clause *, ipartitions_t > > units_to_partition;  // Unit clauses and their partitions
      map< Clause *, ipartitions_t >            clause_to_partition; // Clause id to interpolant partition
#endif
      //
      // Data structures for DTC
      //
      // vector< Enode * >  interface_terms;            // Interface terms for lazy dtc
      // set< Enode * >     interface_terms_cache;      // Interface terms for lazy dtc
      set< PTRef >     interface_equalities;       // To check that we do not duplicate eij
      set< PTRef >     atoms_seen;                 // Some interface equalities may already exists in the formula


      int                next_it_i;                  // Next index i
      int                next_it_j;                  // Next index j
      //
      // Data structures required for incrementality, backtrackability
      //
      enum oper_t
      {
	  NEWVAR
	, NEWUNIT
	, NEWCLAUSE
	, NEWLEARNT
	, NEWAXIOM
#ifdef PRODUCE_PROOF
	, NEWPROOF
	, NEWINTER
#endif
      };
      //
      // Automatic push and pop, for enable undo
      //
      vector< size_t >   undo_stack_size;            // Keep track of stack_oper size
      vector< oper_t >   undo_stack_oper;            // Keep track of operations
      vector< void * >   undo_stack_elem;            // Keep track of aux info
      vector< int >      undo_trail_size;            // Keep track of trail size
      //
      // TODO: move more data in STATISTICS
      //
#ifdef STATISTICS
      double             preproc_time;
      double             tsolvers_time;
      unsigned           elim_tvars;
      unsigned           total_tvars;
      unsigned           ie_generated;
#endif
      bool               init;

      // very debug XXX
#ifdef PEDANTIC_DEBUG
      int                max_dl_debug;
      int                analyze_cnt;
#endif
#ifdef DEBUG_REASONS
      void addTheoryReasonClause_debug(Lit ded, vec<Lit>& reason);
      void checkTheoryReasonClause_debug(Var v);
      void removeTheoryReasonClause_debug(Var v);
#endif
      // Added Code
      //=================================================================================================
  public:
    lbool   lookaheadSplit2(int d, int &idx);
    lbool   lookaheadSplit2(int d);
    void    printTrace() const;

protected:
    virtual inline void clausesPublish(){};
    virtual inline void clausesUpdate(){};
};

//=================================================================================================
// Implementation of inline methods:

inline void CoreSMTSolver::printTrace() const {
    int dl = 0;
    for (int i = 0; i < trail.size(); i++) {
        if (i == 0 || (trail_lim.size() > dl-1 && trail_lim[dl-1] == i))
            printf("\ndl %d:\n  ", dl++);

        printf("%s%d ", sign(trail[i]) ? "-" : "", var(trail[i]));
    }
    printf("\n");
}

inline void CoreSMTSolver::insertVarOrder(Var x) {
    if (!order_heap.inHeap(x) && decision_var[x]) order_heap.insert(x); }

inline void CoreSMTSolver::varDecayActivity() { var_inc *= var_decay; }
inline void CoreSMTSolver::varBumpActivity(Var v) {
   if ( (activity[v] += var_inc) > 1e100 ) {
     // Rescale:
     for (int i = 0; i < nVars(); i++)
       activity[i] *= 1e-100;
     var_inc *= 1e-100; }
     // Update order_heap with respect to new activity:
     if (order_heap.inHeap(v))
       order_heap.decrease(v);
}

//=================================================================================================
// Added Code

inline void CoreSMTSolver::boolVarDecActivity( )
{
#if 1
  if (first_model_found)
    return;
/*
  for (int i = 2; i < nVars(); i++)
  {
    Enode * e = theory_handler->varToEnode( i );
#if 1
    if ( !e->isVar( ) && !first_model_found )
    {
      activity[i] += e->getWeightInc( ) * var_inc;
      // Update order_heap with respect to new activity:
      if (order_heap.inHeap(i))
	order_heap.decrease(i);
    }
#else
    if ( e->isVar( ) && !first_model_found )
    {
      activity[i] += var_inc;
      // Update order_heap with respect to new activity:
      if (order_heap.inHeap(i))
	order_heap.decrease(i);
    }
#endif
  }
*/
#endif
}
/*

#ifdef PRODUCE_PROOF
inline void CoreSMTSolver::checkPartitions( )
{
  if ( config.produce_inter == 0 )
    return;

  unsigned mixed = 0;

  for (int i = 2; i < nVars(); i++)
  {
    Enode * e = theory_handler->varToEnode( i );
    if ( e->getIPartitions( ) == 0 )
      opensmt_error2( "node without partitions: ", e );
    if ( e->getIPartitions( ) % 2 == 1 )
      mixed ++;
  }
}
#endif
*/
// Added Code
//=================================================================================================

inline void CoreSMTSolver::claDecayActivity() { cla_inc *= clause_decay; }
inline void CoreSMTSolver::claBumpActivity (Clause& c) {
  if ( (c.activity() += cla_inc) > 1e20 ) {
    // Rescale:
    for (int i = 0; i < learnts.size(); i++)
      learnts[i]->activity() *= 1e-20;
    cla_inc *= 1e-20; } }

inline bool     CoreSMTSolver::enqueue         (Lit p, Clause* from)   { return value(p) != l_Undef ? value(p) != l_False : (uncheckedEnqueue(p, from), true); }
inline bool     CoreSMTSolver::locked          (const Clause& c) const { return reason[var(c[0])] == &c && value(c[0]) == l_True; }
inline void     CoreSMTSolver::newDecisionLevel() {
  trail_lim.push(trail.size());
#ifdef PEDANTIC_DEBUG
  max_dl_debug = decisionLevel() > max_dl_debug ? decisionLevel() : max_dl_debug;
#endif
 }

inline int      CoreSMTSolver::decisionLevel ()      const                { return trail_lim.size(); }
inline uint32_t CoreSMTSolver::abstractLevel (Var x) const                { return 1 << (level[x] & 31); }
inline lbool    CoreSMTSolver::value         (Var x) const                { return toLbool(assigns[x]); }
inline lbool    CoreSMTSolver::value         (Lit p) const                { return toLbool(assigns[var(p)]) ^ sign(p); }
inline lbool    CoreSMTSolver::modelValue    (Lit p) const                { return model[var(p)] ^ sign(p); }
inline int      CoreSMTSolver::nAssigns      ()      const                { return trail.size(); }
inline int      CoreSMTSolver::nClauses      ()      const                { return clauses.size(); }
inline int      CoreSMTSolver::nLearnts      ()      const                { return learnts.size(); }
inline int      CoreSMTSolver::nVars         ()      const                { return assigns.size(); }
inline void     CoreSMTSolver::setPolarity   (Var v, bool b)              { polarity    [v] = (char)b; }
inline void     CoreSMTSolver::setDecisionVar(Var v, bool b)              { decision_var[v] = (char)b; if (b) { insertVarOrder(v); } }
inline lbool    CoreSMTSolver::solve         ()                           { vec<Lit> tmp; return solve(tmp); }
inline lbool    CoreSMTSolver::solve         ( const vec<Lit> & assumps ) { return solve( assumps, 0 ); }
inline bool     CoreSMTSolver::okay          () const                     { return ok; }

//=================================================================================================
// Debug + etc:

#define reportf(format, args...) ( fflush(stdout), fprintf(stderr, format, ## args), fflush(stderr) )

static inline void logLit(FILE* f, Lit l)
{
  fprintf(f, "%sx%d", sign(l) ? "~" : "", var(l)+1);
}

static inline void logLits(FILE* f, const vec<Lit>& ls)
{
  fprintf(f, "[ ");
  if (ls.size() > 0){
    logLit(f, ls[0]);
    for (int i = 1; i < ls.size(); i++){
      fprintf(f, ", ");
      logLit(f, ls[i]);
    }
  }
  fprintf(f, "] ");
}

static inline const char* showBool(bool b) { return b ? "true" : "false"; }


// Just like 'assert()' but expression will be evaluated in the release version as well.
static inline void check(bool expr) { (void)expr; assert(expr); }

inline void CoreSMTSolver::printLit(Lit l)
{
  reportf("%s%-3d", sign(l) ? "-" : "", var(l)+1 );
  //reportf("%s%-3d:%c", sign(l) ? "-" : "", var(l)+1, value(l) == l_True ? '1' : (value(l) == l_False ? '0' : 'X'));
}


template<class C>
inline void CoreSMTSolver::printClause(const C& c)
{
  for (int i = 0; i < c.size(); i++){
    printLit(c[i]);
    fprintf(stderr, " ");
  }
}

inline void CoreSMTSolver::cnfToString(CnfState& cs)
{
    int curr_dl0_idx = trail_lim.size() > 0 ? trail_lim[0] : trail.size();
    SplitData sd(clauses, trail, curr_dl0_idx);
    sd.updateInstance();
    if (config.sat_dump_learnts()) {
        for (int i = 0; i < learnts.size(); i++)
            sd.addLearnt(*learnts[i]);
    }
    if (okay())
        cs.setCnf(sd.splitToString());
    else cs.setUnsat();
}


//=================================================================================================
// Added code
/*
  template<class C>
inline void CoreSMTSolver::printSMTClause( ostream & os, const C& c )
{
  if ( c.size( ) == 0 ) os << "-";
  if ( c.size( ) > 1 ) os << "(or ";
  for (int i = 0; i < c.size(); i++)
  {
    Var v = var(c[i]);
    if ( v <= 1 ) continue;
    Enode * e = theory_handler->varToEnode( v );
    os << (sign(c[i])?"(not ":"") << e << (sign(c[i])?") ":" ");
  }
  if ( c.size( ) > 1 ) os << ")";
}

inline void CoreSMTSolver::printSMTClause( ostream & os, vec< Lit > & c, bool ids )
{
  if ( c.size( ) == 0 ) os << "-";
  if ( c.size( ) > 1 ) os << "(or ";
  for (int i = 0; i < c.size(); i++)
  {
    Var v = var(c[i]);
    if ( v <= 1 ) continue;
    if ( ids )
      os << (sign(c[i])?"-":" ") << v << " ";
    else
    {
      Enode * e = theory_handler->varToEnode( v );
      os << (sign(c[i])?"(not ":"") << e << (sign(c[i])?") ":" ");
    }
  }
  if ( c.size( ) > 1 ) os << ")";
}

inline void CoreSMTSolver::printSMTClause( ostream & os, vector< Lit > & c, bool ids )
{
  if ( c.size( ) == 0 ) os << "-";
  if ( c.size( ) > 1 ) os << "(or ";
  for (size_t i = 0; i < c.size(); i++)
  {
    Var v = var(c[i]);
    if ( v <= 1 ) continue;
    if ( ids )
      os << (sign(c[i])?"-":" ") << v << " ";
    else
    {
      Enode * e = theory_handler->varToEnode( v );
      os << (sign(c[i])?"(not ":"") << e << (sign(c[i])?") ":" ");
    }
  }
  if ( c.size( ) > 1 ) os << ")";
}

inline void CoreSMTSolver::printSMTLit( ostream & os, const Lit l )
{
  Var v = var( l );
  if ( v == 0 ) os << "true";
  else if ( v == 1 ) os << "false";
  else
  {
    Enode * e = theory_handler->varToEnode( v );
    os << (sign(l)?"(not ":" ") << e << (sign(l)?") ":" ");
  }
}
*/
//ADDED FOR MINIMIZATION
inline void CoreSMTSolver::printClause( vec< Lit > & c )
{
  for (int i = 0; i < c.size(); i++){
    printLit(c[i]);
    fprintf(stderr, " ");
  }
}

#ifdef PRODUCE_PROOF
/*
inline void CoreSMTSolver::printRestrictedSMTClause( ostream & os, vec< Lit > & c
                                                   , const ipartitions_t & mask )
{
  assert( c.size( ) > 0 );
  int nof_lits = 0;
  stringstream s;
  for ( int i = 0 ; i < c.size( ) ; i++ )
  {
    Var v = var(c[i]);
    if ( v <= 1 ) continue;
    Enode * e = theory_handler->varToEnode( v );
    if ( (e->getIPartitions( ) & mask) != 0 )
    {
      s << (sign(c[i])?"(not ":"") << e << (sign(c[i])?") ":" ");
      nof_lits ++;
    }
  }
  if ( nof_lits == 0 )
    os << "false";
  else if ( nof_lits == c.size( ) )
    os << "true";
  else
  {
    if ( nof_lits > 1 ) os << "(or ";
    os << s.str( );
    if ( nof_lits > 1 ) os << ")";
  }
}

inline Enode * CoreSMTSolver::mkRestrictedSMTClause( vec< Lit > & c
                                                   , const ipartitions_t & mask )
{
  assert( c.size( ) > 0 );
  list< Enode * > args;
  for ( int i = 0 ; i < c.size( ) ; i++ )
  {
    Var v = var(c[i]);
    if ( v <= 1 ) continue;

    Enode * e = sign(c[i])
      ? egraph.mkNot( egraph.cons( theory_handler->varToEnode( v ) ) )
      : theory_handler->varToEnode( v );
    Enode * epos = theory_handler->varToEnode( v );
    //
    // If Shared literal
    //
    if ( ((epos->getIPartitions( ) &  mask) != 0)
	&& ((epos->getIPartitions( ) & ~mask) != 0) )
    {
      args.push_front( e );
    }
  }
  if ( args.size( ) == 0 )
    return egraph.mkFalse( );

  return egraph.mkOr( egraph.cons( args ) );
}
*/
#endif

//=================================================================================================

#endif
