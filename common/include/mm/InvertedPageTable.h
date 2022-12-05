#pragma once

#include "types.h"
#include "Mutex.h"
#include "umap.h"
#include "uvector.h"
#include "UserProcess.h"

#define WAS_LAST 0x82426784

struct IPTFlags
{
    bool cow;
    bool shared;
    bool swapped;
};


struct InvertedPageTableEntry
{
    ustl::map<UserProcess*, size_t> progs_mappings;
    IPTFlags my_flags;
};


class InvertedPageTable
{
public:
    InvertedPageTable();
    //~InvertedPageTable();
    static InvertedPageTable* instance();

    //NOTE: LOCK IPT BEFORE ANY OF THESE
    void    addRef(size_t ppn, UserProcess* proc, size_t vpn, IPTFlags* flags = 0);
    size_t  deleteRef(size_t ppn, UserProcess* proc);
    InvertedPageTableEntry* getEntry(size_t ppn);
    IPTFlags* getFlags(size_t ppn);
    
    //locking from outside
    void    lockIPT()                  { IPT_lock_.acquire(); }
    void    unlockIPT()                { IPT_lock_.release(); }
    bool    checkIPT()                 { return IPT_lock_.isHeldBy(currentThread); }

    //for Deduplication Thread
    void deduplicatePages();
    bool deduplicate(size_t page_1, size_t page_2);


private:
    static InvertedPageTable* instance_;
    Mutex IPT_lock_;
    ustl::map<size_t, InvertedPageTableEntry> IPT_;
    InvertedPageTable(InvertedPageTable const&);

};
