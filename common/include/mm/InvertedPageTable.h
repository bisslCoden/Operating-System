#pragma once

#include "types.h"
#include "Mutex.h"
#include "umap.h"
#include "umultimap.h"
#include "uvector.h"
#include "UserProcess.h"
#include "PageReplacementAlgos.h"

#define WAS_LAST 0x82426784

struct IPTFlags
{
    bool cow;
    bool shared;
    bool swapped;
};

//0x446: proc2 -> WAS_LAST

class PageReplacementAlgos;

struct IPTE
{
    ustl::multimap<UserProcess*, size_t> progs_mappings;
    IPTFlags my_flags;
    size_t page_map_level;
};


class InvertedPageTable
{
public:
    InvertedPageTable();
    //~InvertedPageTable();
    static InvertedPageTable* instance();

    //NOTE: LOCK IPT BEFORE ANY OF THESE
    void    addRef(size_t ppn, UserProcess* proc, size_t vpn, IPTFlags* flags = 0, size_t pml = 0);
    size_t  deleteRef(size_t ppn, UserProcess* proc, size_t vpn, size_t pml = 0);
    IPTE* getEntry(size_t ppn);
    bool addEntry(size_t page_number, IPTE entry);
    bool deleteEntry(size_t page_number);
    
    IPTFlags* getFlags(size_t ppn);
    
    //locking from outside
    void    lockIPT()     {  IPT_lock_.acquire(); }
          
    void    unlockIPT()                { IPT_lock_.release(); }
    bool    checkIPT()                 { return IPT_lock_.isHeldBy(currentThread); }

    Thread* whoHasLock() { return IPT_lock_.heldBy(); }

    //for Deduplication Thread
    void deduplicatePages();
    bool deduplicate(size_t page_1, size_t page_2);
    size_t computeChecksum(size_t* start);
    //size_t getIPTSize()  { return IPT_.size(); };


private:
    friend class PageReplacementAlgos;
    static InvertedPageTable* instance_;
    Mutex IPT_lock_;
    ustl::map<size_t, IPTE> IPT_;
    InvertedPageTable(InvertedPageTable const&);

};
