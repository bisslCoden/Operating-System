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


void InvertedPageTable::addRef(size_t ppn, UserProcess* proc, size_t vpn, IPTFlags* flags, size_t pml)
{
  assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE Lock IPT()!!!!");
  
  if (IPT_.find(ppn) == IPT_.end())
  {
    InvertedPageTableEntry new_entry;
    if (flags)
      new_entry.my_flags = { flags->cow, flags->shared, flags->swapped };
    else
      new_entry.my_flags = { false, false, false };
   
    new_entry.progs_mappings.insert(ustl::make_pair(proc, vpn));
    new_entry.page_map_level = pml;
    IPT_.emplace(ppn, new_entry);
  }
  else
  {
    if (pml != IPT_[ppn].page_map_level)
      assert(false && "NOPE! dont mix pagemaplevels on a page!\n");
    
    auto my_proc = IPT_[ppn].progs_mappings.find(proc);
    if (my_proc != IPT_[ppn].progs_mappings.end())
    {
      if (my_proc->second == vpn)
      {
        assert(false && "tried to add same proc same vpn twice ... i should assert\n");
      }
    }

    if (flags)
        IPT_[ppn].my_flags = { flags->cow, flags->shared, flags->swapped };
    
    IPT_[ppn].progs_mappings.insert(ustl::make_pair(proc, vpn));
    debug(X_USERPROCESS, "[%ld]added Proc %ld to page %lx size is then %ld\n", currentThread->getTID(), proc->getPID(), ppn, IPT_[ppn].progs_mappings.size());
  }
}

size_t InvertedPageTable::deleteRef(size_t ppn, UserProcess* proc, size_t vpn, size_t pml)
{
  assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE lockIPT()!!!!");
  assert(proc != 0);
  debug(X_USERPROCESS, "proc [%ld] for page %lx on level %ld: tries to delete ref...\n", proc->getPID(), ppn, pml);
  if (IPT_.find(ppn) == IPT_.end())
  {
    debug(X_USERPROCESS, "[%ld] proc [%ld] Wtf? Tried to free a page which is not in the IPT anymore forgot to set present to 0 somewhere!!!\n", currentThread->getTID(), 
    proc->getPID());
    return 0;
  }
  else
  {
    if (pml != IPT_[ppn].page_map_level)
      assert(false && "NOPE! dont mix pagemaplevels on a page!\n");
    
    auto my_proc = IPT_[ppn].progs_mappings.find(proc);
    
    if (my_proc != IPT_[ppn].progs_mappings.end())
    {
      debug(X_USERPROCESS, "[%ld] found my entry vpn: %lx ppn: %lx (searched vpn: %lx)\n", my_proc->first->getPID(), my_proc->second, ppn, vpn);
      if (pml == 0)
      {
        while (my_proc->second != vpn)
        {
          my_proc++;
          if (my_proc->first != proc)
          {
            assert(false && "my ref is not in here for THAT vpn\n");
          }
        }  
      }
      
      if (IPT_[ppn].my_flags.cow)
      {
        int cow_if_del = (int)IPT_[ppn].progs_mappings.size() - 1;
        if (cow_if_del == 0)
        {
          IPT_[ppn].my_flags.cow = false;
          return WAS_LAST;
        }
      }

      IPT_[ppn].progs_mappings.erase(my_proc);
      debug(X_USERPROCESS, "erasing [%ld] vpn: %lx from %lx\n", my_proc->first->getPID(), vpn, ppn);
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
  size_t ddp1, ddp2, checksum1, checksum2;
  //auto end = IPT_.end();
  for (auto iter1 = IPT_.begin(); iter1 <= IPT_.end(); iter1++)
  {
    lockIPT();
    //debug(DEDUBLI_THREAD, "iter1: %lx\n", iter1);
    if (IPT_.empty())
    {
      unlockIPT();
      return;
    }
    // assert((size_t) iter1 != 0xffffffff80490010 && "found our mysterious adress iter 1!\n");
    // assert((size_t) iter1 != 0xffffffff80490000 && "found our mysterious adress iter 1!\n");
    
    ddp1 = iter1->first;
    if(IPT_.find(ddp1) != IPT_.end())
    {
      checksum1 = computeChecksum((size_t*) ArchMemory::getIdentAddressOfPPN(ddp1));
      unlockIPT();
    }
    else
    {
      unlockIPT();
      continue;
    }

    for (auto iter2 = IPT_.begin(); iter2 <= IPT_.end(); iter2++)
    {
     // debug(DEDUBLI_THREAD, "iter 1 at page %lx iter 2 at page %lx\n", iter1->first, iter2->first);
     // debug(DEDUBLI_THREAD, "iter2: %lx\n", iter2);
      lockIPT();
      // assert((size_t) iter2 != 0xffffffff80490010 && "found our mysterious adress iter 2!\n");
      // assert((size_t) iter2 != 0xffffffff80490000 && "found our mysterious adress iter 2!\n");

      if (IPT_.empty())
      {
        unlockIPT();
        return;
      }
      ddp2 = iter2->first;
      if (IPT_.find(ddp2) != IPT_.end())
      {
        if (ddp1 == ddp2)
        {
          unlockIPT();
          debug(DEDUBLI_THREAD, "skipping this one\n");
        //  end = IPT_.end();
          continue;
        }
        checksum2 = computeChecksum((size_t*) ArchMemory::getIdentAddressOfPPN(iter2->first));
        unlockIPT();
        if (checksum1 == checksum2)
        {
          debug(DEDUBLI_THREAD, "[deduplication found 2 equal pages!]\n");
          if(deduplicate(ddp1, ddp2))
            debug(DEDUBLI_THREAD, "deduplication of page %lx and page %lx worked\n", ddp1, ddp2);
          else
          {
            debug(DEDUBLI_THREAD, "somehow could not ddp page %lx and page %lx\n", ddp1, ddp2);
          //  end = IPT_.end();
            break;
          }
        }
        else 
        {
      //    end = IPT_.end();
          continue;
        }
      }
      else 
      {
        unlockIPT();
   //     end = IPT_.end();
        break;
      }
    }
  //  end = IPT_.end();
  }
}

size_t InvertedPageTable::computeChecksum(size_t* start)
{
  size_t* checkiter = start;
  size_t checksum = 0;
  for (size_t i = 0; i < PAGE_SIZE / 8; i++)
  {
    checksum += *checkiter;
    checkiter++; 
  }
  return checksum;
}


bool InvertedPageTable::deduplicate(size_t page_1, size_t page_2)
{
 // lockIPT();
  //add effected progs
  ustl::map<UserProcess*, size_t> progs;
  ArchMemoryMapping m;
  
  debug(DEDUBLI_THREAD, "Pages %lx and %lx are equal! we have %ld progs on first and %ld on second page\n", page_1, page_2, 
  IPT_[page_1].progs_mappings.size(), IPT_[page_2].progs_mappings.size());
 
  bool prog_safe = false;
  while (!prog_safe)
  {
    lockIPT();
    progs.clear();
    if (IPT_.find(page_1) == IPT_.end() || IPT_.find(page_2) == IPT_.end())
    {
      unlockIPT();
      return false;
    }
    else if(IPT_[page_1].page_map_level != IPT_[page_2].page_map_level)
    {
      unlockIPT();
      return false;
    }
    for (auto prog : IPT_[page_1].progs_mappings)
    {
      debug(DEDUBLI_THREAD, "emplacing [%ld]\n", prog.first->getPID());
      progs.emplace(prog);
    }
    //0x7acad0e1e000
    //0x7acad0e1ef90
    //progs.emplace((UserProcess*) 0,(size_t) 0);
    for (auto prog : IPT_[page_2].progs_mappings)
    {
      debug(DEDUBLI_THREAD, "emplacing [%ld]\n", prog.first->getPID());
      progs.emplace(prog);
    }
    //this means all progs are there and their archmems are locked <3
    prog_safe = ProcessRegistry::instance()->lockMultArchmem(progs);
    if (!prog_safe)
    {
      debug(DEDUBLI_THREAD, "seems like not all progs are ready... i ll try again later\n");
      unlockIPT();
      Scheduler::instance()->yield();
    }
  }
  
  debug(DEDUBLI_THREAD, "locking sucessful!\n");
  // -> now no more writes allowed from heeere... we only need to do this for lowest lvl bc for the higher levels writes
  //need locks anyways...
  if (IPT_[page_1].page_map_level == 0)
  {
    for (auto prog : progs)
    {
      m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
      PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
      //debug(DEDUBLI_THREAD, "changing page entry on: [%lx]\n", m.pd[m.pdi].pt.page_ppn);
      pt_src[m.pti].writeable = 0;
    }
  }
  
  //moment of truth...
  void* finalcheck1 = (void*) ArchMemory::getIdentAddressOfPPN(page_1);
  void* finalcheck2 = (void*) ArchMemory::getIdentAddressOfPPN(page_2);
  if (memcmp(finalcheck1, finalcheck2, PAGE_SIZE) == 0)
  {
    //yes! I can actually deduplicate something...
    debug(DEDUBLI_THREAD, "Pages still equal! now dedublication\n");
    //level one case: this is easy!
    if (IPT_[page_1].page_map_level == 0)
    {
      if (!IPT_[page_1].my_flags.cow)
      {
        for (auto prog : IPT_[page_1].progs_mappings)
        {
          debug(DEDUBLI_THREAD, "Proc: [%ld], vpn %lx is on %d page\n",prog.first->getPID(), prog.second, 1);
          if (!prog.first->getLoader())
            assert(false);
          
          ArchMemory* archmem = &(prog.first->getLoader()->arch_memory_);
          assert(archmem->checkforPMLCow(prog.second, true) && "somthing not present shouldnt happen on lv0");
          m = archmem->resolveMapping(prog.second);
          PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt_ppn);
          pt_src[m.pti].writeable = 0;
          pt_src[m.pti].cow = 1;
        }
      IPT_[page_1].my_flags.cow = true;
      }
    
      for (auto prog : IPT_[page_2].progs_mappings)
      {
        debug(DEDUBLI_THREAD, "Proc: [%ld], vpn %lx is on %d page\n",prog.first->getPID(), prog.second, 2);
        if (!prog.first->getLoader())
          assert(false);
        
        ArchMemory* archmem = &(prog.first->getLoader()->arch_memory_);
        assert(archmem->checkforPMLCow(prog.second, true) && "somthing not present shouldnt happen on lv0");
        m = archmem->resolveMapping(prog.second);
        PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt_ppn);
        pt_src[m.pti].writeable = 0;
        pt_src[m.pti].cow = 1;
        pt_src[m.pti].page_ppn = page_1;
        addRef(page_1, prog.first, prog.second);
      }
    }
    else
    {
      int level = IPT_[page_1].page_map_level;
      PageMapLevel4Entry* pml4;
      PageDirPointerTableEntry* pdpt;
      PageDirEntry* pd;
      debug(DEDUBLI_THREAD, "Now it gets interesting for pages %lx and %lx! found PML %d Cow\n", page_1, page_2, level);
      if (!IPT_[page_1].my_flags.cow)
      {
        for (auto prog : IPT_[page_1].progs_mappings)
        {
          if (!prog.first->getLoader())
            assert(false);
          ArchMemory* archmem = &(prog.first->getLoader()->arch_memory_);
          ustl::pair<size_t, size_t> ppn_index = archmem->cowUntil(page_1, level);
          assert(ppn_index.first != 0);
          debug(DEDUBLI_THREAD, "Proc: [%ld] pml is on %ld and my index at %ld were on page 1\n",prog.first->getPID(), ppn_index.first, ppn_index.second);
       
          switch (level)
          {
          case 3:
            pml4 = (PageMapLevel4Entry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
            pml4[ppn_index.second].cow = 1;
            pml4[ppn_index.second].writeable = 0;
            break;
          case 2:
            pdpt = (PageDirPointerTableEntry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
            pdpt[ppn_index.second].pd.cow = 1;
            pdpt[ppn_index.second].pd.writeable = 0;
            break;
          case 1:
            pd = (PageDirEntry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
            pd[ppn_index.second].pt.cow = 1;
            pd[ppn_index.second].pt.writeable = 0;
            break;
          default:
            assert(false && "which level should this be??\n");
            break;
          }
        }
        IPT_[page_1].my_flags.cow = true;
      }
      for (auto prog : IPT_[page_2].progs_mappings)
      {
        if (!prog.first->getLoader())
          assert(false);
        ArchMemory* archmem = &(prog.first->getLoader()->arch_memory_);
        ustl::pair<size_t, size_t> ppn_index = archmem->cowUntil(page_2, level);
        assert(ppn_index.first != 0);
        debug(DEDUBLI_THREAD, "Proc: [%ld] pml is on %lx and my index at %ld were on page 2\n",prog.first->getPID(), ppn_index.first, ppn_index.second);
        switch (level)
        {
        case 3:
          pml4 = (PageMapLevel4Entry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
          pml4[ppn_index.second].cow = 1;
          pml4[ppn_index.second].writeable = 0;
          pml4[ppn_index.second].page_ppn = page_1;
          break;
        case 2:
          pdpt = (PageDirPointerTableEntry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
          pdpt[ppn_index.second].pd.cow = 1;
          pdpt[ppn_index.second].pd.writeable = 0;
          pdpt[ppn_index.second].pd.page_ppn = page_1;
          break;
        case 1:
          pd = (PageDirEntry*) ArchMemory::getIdentAddressOfPPN(ppn_index.first);
          pd[ppn_index.second].pt.cow = 1;
          pd[ppn_index.second].pt.writeable = 0;
          pd[ppn_index.second].pt.page_ppn = page_1;
          break;
        default:
          assert(false && "which level should this be??\n");
          break;
        }
      }
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
    if (IPT_[page_1].page_map_level == 0)
    {
      for (auto prog : progs)
      {
        m = prog.first->getLoader()->arch_memory_.resolveMapping(prog.second);
        PageTableEntry* pt_src = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
        pt_src[m.pti].writeable = 1;
      } 
    }
    
    ProcessRegistry::instance()->unlockMultArchmem(progs);
    unlockIPT();
    return false;
  }
}



