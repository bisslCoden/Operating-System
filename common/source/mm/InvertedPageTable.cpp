#include "InvertedPageTable.h"
#include "debug.h"
#include "ProcessRegistry.h"
#include "PageManager.h"

InvertedPageTable IPT;
InvertedPageTable* InvertedPageTable::instance_ = 0;

InvertedPageTable* InvertedPageTable::instance()
{
    if (unlikely(!instance_))
        new (&IPT) InvertedPageTable();
    return instance_;
}

InvertedPageTable::InvertedPageTable() : IPT_lock_("InvertedPageTable::lock_")
{
    assert(instance_ == 0);
    instance_ = this;
}


void InvertedPageTable::addRef(size_t ppn, UserProcess* proc, size_t vpn, IPTFlags* flags)
{
  assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE Lock IPT()!!!!");
  
  if (IPT_.find(ppn) == IPT_.end())
  {
    InvertedPageTableEntry new_entry;
    if (flags)
      new_entry.my_flags = { flags->cow, flags->shared, flags->swapped };
    else
      new_entry.my_flags = { false, false, false };
   
    new_entry.progs_mappings.emplace(proc, vpn);
    IPT_.emplace(ppn, new_entry);
  }
  else
  {
    if (IPT_[ppn].progs_mappings.find(proc) != IPT_[ppn].progs_mappings.end())
    {
        assert(false && "tried to add same proc twice ... not sure if i should assert\n");
    }
    if (flags)
        IPT_[ppn].my_flags = { flags->cow, flags->shared, flags->swapped };
    
    IPT_[ppn].progs_mappings.emplace(proc, vpn);
    debug(X_USERPROCESS, "[%ld]added Proc %ld to page %lx size is then %ld\n", currentThread->getTID(), proc->getPID(), ppn, IPT_[ppn].progs_mappings.size());
  }
}

size_t InvertedPageTable::deleteRef(size_t ppn, UserProcess* proc)
{
  assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE lockIPT()!!!!");

  if (IPT_.find(ppn) == IPT_.end())
  {
    debug(X_USERPROCESS, "[%ld] proc [%ld] Wtf? Tried to free a page which is not in the IPT anymore forgot to set present to 0 somewhere!!!\n", currentThread->getTID(), 
    proc->getPID());
    return 0;
  }
  else
  {
    if (IPT_[ppn].progs_mappings.find(proc) != IPT_[ppn].progs_mappings.end())
    {
      if (IPT_[ppn].my_flags.cow)
      {
        int cow_if_del = (int)IPT_[ppn].progs_mappings.size() - 1;
        if (cow_if_del == 0)
        {
          IPT_[ppn].my_flags.cow = 0;
          return WAS_LAST;
        }
      }
      IPT_[ppn].progs_mappings.erase(proc);
      if (IPT_[ppn].progs_mappings.size() == 0)
      {
        IPT_.erase(ppn);
        return 0;
      }
      else
        return IPT_[ppn].progs_mappings.size();
    }
    else
      debug(X_USERPROCESS, "[%ld] Tried to delete ref for proc [%ld] on page [%lx] even though someone else did already for me\n", currentThread->getTID(), proc->getPID(), ppn);
    return -1;
  }
}

InvertedPageTableEntry* InvertedPageTable::getEntry(size_t ppn)
{
  if (IPT_.find(ppn) != IPT_.end())
    return &IPT_[ppn];
  return 0;
}

IPTFlags* InvertedPageTable::getFlags(size_t ppn)
{
  if (IPT_.find(ppn) != IPT_.end())
  {
    return &IPT_[ppn].my_flags;
  }
  return 0;
}


//------------------------------------------------------------------------------------------------
//Dedupli Thread Functions
//------------------------------------------------------------------------------------------------

void InvertedPageTable::deduplicatePages()
{
  //this is not threadsafe! but that is not the goal: we want first a lightweight comparison
  // we do all the heavy locking after that when we really get serious with deduplicating
  size_t ddp1, ddp2;
  for (auto iter1 = IPT_.begin(); iter1 != IPT_.end(); iter1++)
  {
    ddp1 = iter1->first;
    void* this_page = (void*) ArchMemory::getIdentAddressOfPPN(iter1->first);
    auto iter2 = iter1;
    iter2++;
    for (; iter2 < IPT_.end(); iter2++)
    {
      ddp2 = iter2->first;
      void* currentPage = (void*) ArchMemory::getIdentAddressOfPPN(iter2->first);
      if (memcmp(this_page, currentPage, PAGE_SIZE) == 0)
      {
        debug(DEDUBLI_THREAD, "[deduplication found 2 equal pages!]\n");
        if(deduplicate(ddp1, ddp2))
          debug(DEDUBLI_THREAD, "deduplication of page %lx and page %lx worked\n", ddp1, ddp2);
        else
          debug(DEDUBLI_THREAD, "somehow could not ddp page %lx and page %lx\n", ddp1, ddp2);
      }
    }
  }
  
}


bool InvertedPageTable::deduplicate(size_t page_1, size_t page_2)
{
  lockIPT();
  //add effected progs
  ustl::map<UserProcess*, size_t> progs;
  debug(DEDUBLI_THREAD, "Pages %lx and %lx are equal! we have %ld progs on first and %ld on second page\n", page_1, page_2, 
  IPT_[page_1].progs_mappings.size(), IPT_[page_2].progs_mappings.size());
  for (auto prog : IPT_[page_1].progs_mappings)
  {
    debug(DEDUBLI_THREAD, "emplacing [%ld]\n", prog.first->getPID());
    progs.emplace(prog);
  }

  //progs.emplace((UserProcess*) 0,(size_t) 0);
  for (auto prog : IPT_[page_2].progs_mappings)
  {
    debug(DEDUBLI_THREAD, "emplacing [%ld]\n", prog.first->getPID());
    progs.emplace(prog);
  }
  
  ProcessRegistry::instance()->lockMultArchmem(progs);
  debug(DEDUBLI_THREAD, "locking sucessful!\n");
  // -> now no more writes allowed from heeere...
  for (auto prog : progs)
  {
    ArchMemoryMapping m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
    PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt->page_ppn);
    pt_src[m.pti].writeable = 0;
  }
  //moment of truth...
  void* finalcheck1 = (void*) ArchMemory::getIdentAddressOfPPN(page_1);
  void* finalcheck2 = (void*) ArchMemory::getIdentAddressOfPPN(page_2);
  if (memcmp(finalcheck1, finalcheck2, PAGE_SIZE) == 0)
  {
    //yes! I can actually deduplicate something...
    debug(DEDUBLI_THREAD, "Pages still equal! now dedublication\n");
    IPT_[page_1].my_flags.cow = true;
    
    for (auto prog : IPT_[page_1].progs_mappings)
    {
      debug(DEDUBLI_THREAD, "Proc: [%ld], vpn %lx is on %d page\n",prog.first->getPID(), prog.second, 1);
      if (!prog.first->getLoader())
      {
        assert(false);
      }
      
      ArchMemoryMapping m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
      PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt->page_ppn);
      pt_src[m.pti].writeable = 0;
      pt_src[m.pti].cow = 1;
      
    }
    for (auto prog : IPT_[page_2].progs_mappings)
    {
      debug(DEDUBLI_THREAD, "Proc: [%ld], vpn %lx is on %d page\n",prog.first->getPID(), prog.second, 2);
      if (!prog.first->getLoader())
      {
        assert(false);
      }
      ArchMemoryMapping m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
      PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt->page_ppn);
      pt_src[m.pti].writeable = 0;
      pt_src[m.pti].cow = 1;
      pt_src[m.pti].page_ppn = page_1;
      addRef(page_1, prog.first, prog.second);
    }

    IPT_.erase(page_2);
    PageManager::instance()->freePPN(page_2);
    ProcessRegistry::instance()->unlockMultArchmem(progs);
    unlockIPT();
    return true;
  }
  else
  {
    //damn... it changed
    for (auto prog : progs)
    {
      ArchMemoryMapping m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
      PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt->page_ppn);
      pt_src[m.pti].writeable = 1;
    } 
    ProcessRegistry::instance()->unlockMultArchmem(progs);
    unlockIPT();
    return false;
  }
}



