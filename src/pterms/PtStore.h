#ifndef PTSTORE_H
#define PTSTORE_H

#include "Pterm.h"
#include "SymStore.h"
#include "SStore.h"

//struct PTRefHash {
//    uint32_t operator () (const PTRef s) const {
//        return (uint32_t)s; }
//};



class PtStore {
    PtermAllocator pta;
    SymStore&      symstore;
    SStore&        sortstore;

    Map<SymRef,PTRef,SymRefHash,Equal<SymRef> > cterm_map; // Mapping constant symbols to terms
    Map<PTRef,PTRef,PTRefHash,Equal<PTRef> >    cplx_map;  // Mapping complex terms to canonical terms
  public:
    PtStore(SymStore& symstore_, SStore& sortstore_);
    PTRef insertTerm(SymRef sym, const vec<PTRef>& terms) {
        // Catch the constants here
        if (terms.size() == 0 && cterm_map.contains(sym))
            return cterm_map[sym];

        PTRef res = pta.alloc(sym, terms);
//        printf("Inserting sym %d (%s) having %d args.  The term reference is %d\n", sym, symstore.getName(sym), terms.size(), res);
        if (terms.size() == 0) cterm_map.insert(sym, res);

        return res;
    }

    // XXX Not a good name, since this creates the term
    PTRef lookupTerm(const char* s, const vec<PTRef>& args);

    Pterm& operator[] (PTRef tr) { return pta[tr]; }
    const Pterm& operator[] (PTRef tr) const { return pta[tr]; }

    char* printTerm(PTRef, bool ext = false) const;
    char* printTerm_(PTRef, bool ext = false) const;
};

#endif
