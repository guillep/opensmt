/*********************************************************************
Author: Roberto Bruttomesso <roberto.bruttomesso@gmail.com>

OpenSMT -- Copyright (C) 2009, Roberto Bruttomesso

OpenSMT is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenSMT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenSMT. If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef CNFIZER_H
#define CNFIZER_H

#include "TermMapper.h"
#include "Global.h"
//#include "Otl.h"
//#include "SMTSolver.h"
#include "SimpSMTSolver.h"
#include "Egraph.h"
#include "THandler.h"
#include "PtStore.h"
#include "SStore.h"
#include "Logic.h"

// A small typechecked class for checkClause type return values to enable us to check whether the formula is unsatisfiable simultaneously
class ckval {
    char value;
    explicit ckval(int v) : value(v) { }
public:
    ckval()          : value(0) { }
    int toInt(void) const { return value; }
    bool operator == (ckval c) const { return value == c.value; }
    bool operator != (ckval c) const { return value == c.value; }

    friend ckval toCkval(int v);
};
inline ckval toCkval(int v) {return ckval(v); }

const ckval ck_True  = toCkval( 1);
const ckval ck_False = toCkval(-1);
const ckval ck_Unsat = toCkval( 0);

class ValPair {
  private:
    PTRef       t;
    lbool       v;
  public:
    ValPair(PTRef term, lbool value) : t(term), v(value) {}
    PTRef getTerm()       const { return t; }
    lbool getVal ()       const { return v; }
};

//
// Generic class for conversion into CNF
//
class Cnfizer
{
protected:
    SMTConfig&          config;
    SStore &            sstore;
    TStore&             symstore;
    PtStore&            ptstore;         // Reference to the term store
    Logic&              logic;

    TermMapper          tmap;            // Map vars to proper terms
    THandler            thandler;
    SimpSMTSolver       solver;
    Egraph              uf_solver;

public:

    Cnfizer( PtStore &   ptstore_
           , SMTConfig & config_
           , TStore&     symstore_
           , SStore &    sstore_
           , Logic&      logic_
           );


    virtual ~Cnfizer( ) { }

    lbool cnfizeAndGiveToSolver ( PTRef
#ifdef PRODUCE_PROOF
                                , const ipartitions_t = 0
#endif
                                ); // Main routine

    vec<ValPair>* getModel ();                              // Retrieves the model (if SAT and solved)

    lbool  getTermValue(PTRef);

    void   initialize      () { solver.initialize(); }
    lbool  solve           () { status = solver.solve(); return status; }
    lbool  getStatus       () { return status; }

protected:

    virtual bool cnfize                 ( PTRef
#ifdef PRODUCE_PROOF
                                        , const ipartitions_t = 0
#endif
                                        ) = 0; // Actual cnfization. To be implemented in derived classes

    bool     deMorganize                ( PTRef
#ifdef PRODUCE_PROOF
                                        , const ipartitions_t &
#endif
                                        );                                    // Apply deMorgan rules whenever feasible
//  Enode *  rewriteMaxArity            ( Enode *, map< enodeid_t, int > & );   // Rewrite terms using maximum arity

    bool     checkCnf                   ( PTRef );                            // Check if formula is in CNF
    bool     checkDeMorgan              ( PTRef );                            // Check if formula can be deMorganized
    bool     giveToSolver               ( PTRef
#ifdef PRODUCE_PROOF
                                        , const ipartitions_t &
#endif
                                        );                              // Gives formula to the SAT solver

    void  retrieveTopLevelFormulae   ( PTRef, vec<PTRef> & );         // Retrieves the list of top-level formulae

//  void  retrieveClause             ( PTRef, vec<PTRef> & );         // Retrieve a clause from a formula
//  void  retrieveConjuncts          ( PTRef, vec<PTRef> & );         // Retrieve the list of conjuncts

//  Enode * toggleLit		   ( Enode * );                              // Handy function for toggling literals



private:

//  void    computeIncomingEdges ( Enode *, map< enodeid_t, int > & );         // Computes the list of incoming edges for a node
//  Enode * mergeEnodeArgs       ( Enode *
//                               , map< enodeid_t, Enode * > &
//                               , map< enodeid_t, int > & );                  // Subroutine for rewriteMaxArity

    bool    checkConj            (PTRef, Map<PTRef,bool,TRefHash,Equal<PTRef> >& check_cache); // Check if a formula is a conjunction
    bool    checkClause          (PTRef, Map<PTRef,bool,TRefHash,Equal<PTRef> >& check_cache); // Check if a formula is a clause
    bool    checkPureConj        (PTRef, Map<PTRef,bool,TRefHash,Equal<PTRef> >& check_cache); // Check if a formula is purely a conjuntion

    lbool   status;     // The status of the last solver call (initially l_Undef)

    // The special boolean symbols
protected:

    Map<PTRef,bool,PTRefHash,Equal<PTRef> >   processed;  // Is a term already processed
    Map<PTRef,Var,PTRefHash,Equal<PTRef> >    seen;       // mapping from PTRef to var

    bool  isLit            (PTRef r);
    const Lit findLit      (PTRef ptr);
    bool  isBooleanOperator(TRef tr) { return logic.isBooleanOperator(tr); } // (tr == logic.getSym_and()) | (tr == logic.getSym_or() ) | (tr == logic.getSym_not() ) | (tr == logic.getSym_eq() ) | (tr == logic.getSym_xor() ); }
    bool  isTheorySymbol   (TRef tr) const { return logic.isTheorySymbol(tr); }
    bool  isIte            (TRef tr) const { return logic.isIte(tr); }
    bool  isAtom           (PTRef r) const;
    bool  isNPAtom         (PTRef r, PTRef& p)    const; // Check if r is a (negated) atom.  Return true if the corresponding atom is negated.  The purified reference is placed in the second argument.
    void  declareAtom      (PTRef, TRef);                // Declare an atom for the smt/sat solver
    bool  termSeen         (PTRef)                const; // True if the term has been seen and thus processed in the sense that there is already literal corresponding to it.  Sees through negations.

};

#endif
