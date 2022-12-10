#include "ArchMemory.h"
#include "ArchInterrupts.h"
#include "kprintf.h"
#include "assert.h"
#include "InvertedPageTable.h"
#include "ProcessRegistry.h"
#include "kstring.h"
#include "PageManager.h"
#include "ArchThreads.h"
#include "Thread.h"

PageMapLevel4Entry kernel_page_map_level_4[PAGE_MAP_LEVEL_4_ENTRIES] __attribute__((aligned(0x1000)));
PageDirPointerTableEntry kernel_page_directory_pointer_table[2 * PAGE_DIR_POINTER_TABLE_ENTRIES] __attribute__((aligned(0x1000)));
PageDirEntry kernel_page_directory[2 * PAGE_DIR_ENTRIES] __attribute__((aligned(0x1000)));
PageTableEntry kernel_page_table[8 * PAGE_TABLE_ENTRIES] __attribute__((aligned(0x1000)));


ArchMemory::ArchMemory() :
  arch_memory_lock_("ArchMemory::arch_memory_lock_")
{
  debug(X_ARCHMEM, "Archmemory constructor\n");
  page_map_level_4_ = PageManager::instance()->allocPPN();
  PageMapLevel4Entry* new_pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  memcpy((void*) new_pml4, (void*) kernel_page_map_level_4, PAGE_SIZE);
  memset(new_pml4, 0, PAGE_SIZE / 2); // should be zero, this is just for safety
  debug(X_ARCHMEM, "pml4 lies at %lx on ppn %lx\n", (size_t)new_pml4, page_map_level_4_);
}

template<typename T>
bool ArchMemory::checkAndRemove(pointer map_ptr, uint64 index)
{
  T* map = (T*) map_ptr;
  debug(A_MEMORY, "%s: page %p index %zx\n", __PRETTY_FUNCTION__, map, index);
  ((uint64*) map)[index] = 0;
  for (uint64 i = 0; i < PAGE_DIR_ENTRIES; i++)
  {
    if (map[i].present != 0)
      return false;
  }
  return true;
}
//now starting to break our working COW
bool ArchMemory::unmapPage(uint64 virtual_page)
{
  InvertedPageTable* IPT = InvertedPageTable::instance();
  ArchMemoryMapping m = resolveMapping(virtual_page);

  assert(m.page_ppn != 0 && m.page_size == PAGE_SIZE && m.pt[m.pti].present);
  assert(checkforPMLCow(virtual_page, true) && "pmlcow didnt work for some reason...\n");
  //need to resolve again bc something might  have changed...
  m = resolveMapping(virtual_page);

  PageTableEntry* pt_ident  = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pt_ppn);
  pt_ident[m.pti].present = 0;
  
  // free if counter not in map or after descresing counter = 0.
  int ret = IPT->deleteRef(m.page_ppn, my_proc, virtual_page);
  
  if(ret == 0)
  {
    debug(X_USERTHREAD, "[%ld] ~unmapPage(): will call freePPN(ppn = %lx)\n", currentThread->getTID(), m.page_ppn);
    PageManager::instance()->freePPN(m.page_ppn);
  }
  else if(ret == (int) WAS_LAST)
  {
    IPT->deleteRef(m.page_ppn, my_proc, virtual_page);
    PageManager::instance()->freePPN(m.page_ppn);
  }
  else
    debug(X_USERTHREAD, "[%ld] ~unmapPage(): COULD NOT CALL FREE freePPN(ppn = %lx)\n", currentThread->getTID(), m.page_ppn);
  
  //pm->unlockIPT();
  ((uint64*)m.pt)[m.pti] = 0; // for easier debugging
  bool empty = checkAndRemove<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti);
  if (empty)
  {
    empty = checkAndRemove<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pt_ppn = %zx)\n", virtual_page, m.pt_ppn);
    ret = IPT->deleteRef(m.pt_ppn, my_proc, 0, 1);
    if (ret == 0)
      PageManager::instance()->freePPN(m.pt_ppn);
    else if (ret == (int) WAS_LAST)
    {
      IPT->deleteRef(m.pt_ppn, my_proc, 0, 1);
      PageManager::instance()->freePPN(m.pt_ppn);
    }
  }
  if (empty)
  {
    empty = checkAndRemove<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pd_ppn = %zx)\n", virtual_page, m.pd_ppn);
    ret = IPT->deleteRef(m.pd_ppn, my_proc, 0, 2);
    if (ret == 0)
      PageManager::instance()->freePPN(m.pd_ppn);
    else if (ret == (int) WAS_LAST)
    {
      IPT->deleteRef(m.pd_ppn, my_proc, 0, 2);
      PageManager::instance()->freePPN(m.pd_ppn);
    }
  }
  if (empty)
  {
    empty = checkAndRemove<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn), m.pml4i);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pdpt_ppn = %zx)\n", virtual_page, m.pdpt_ppn);
    ret = IPT->deleteRef(m.pdpt_ppn, my_proc, 0, 3);
    if (ret == 0)
      PageManager::instance()->freePPN(m.pdpt_ppn);
    else if (ret == (int) WAS_LAST)
    {
      ret = IPT->deleteRef(m.pdpt_ppn, my_proc, 0, 3);
      PageManager::instance()->freePPN(m.pdpt_ppn);
    }
  }
  //unlockArchMemory();
  return true;
}

template<typename T>
bool ArchMemory::insert(pointer map_ptr, uint64 index, uint64 ppn, uint64 bzero, uint64 size, uint64 user_access,
                        uint64 writeable)
{
  assert(map_ptr & ~0xFFFFF00000000000ULL);
  T* map = (T*) map_ptr;
  debug(X_ARCHMEM, "%s: page %p index %zx ppn %zx user_access %zx size %zx\n", __PRETTY_FUNCTION__, map, index, ppn,
        user_access, size);
  if (bzero)
  {
    memset((void*) getIdentAddressOfPPN(ppn), 0, PAGE_SIZE);
    assert(((uint64* )map)[index] == 0);
  }
  map[index].size = size;
  map[index].writeable = writeable;
  map[index].page_ppn = ppn;
  map[index].user_access = user_access;
  map[index].present = 1;
  return true;
}

//check if proc is dead!
bool ArchMemory::mapPage(uint64 virtual_page, uint64 physical_page, uint64 user_access)
{
  InvertedPageTable* IPT = InvertedPageTable::instance();
  debug(A_MEMORY, "%zx %zx %zx %zx\n", page_map_level_4_, virtual_page, physical_page, user_access);

  if (!IPT->checkIPT())
    IPT->lockIPT();
  
  if (!checkArchMemory(currentThread))
    lockArchMemory();
  

  ArchMemoryMapping m = resolveMapping(page_map_level_4_, virtual_page);
  assert((m.page_size == 0) || (m.page_size == PAGE_SIZE));
  
  if(!checkforPMLCow(virtual_page, true))
    debug(MULTICOW, "well seems like we ll have to add some more levels... good thing i cowed everything we have so far...\n");
  debug(MULTICOW, "cowed all levels...\n");
  m = resolveMapping(page_map_level_4_, virtual_page);
 // size_t ppn;
  if (m.pdpt_ppn == 0)
  {
    // ppn = cowPML<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn), m.pml4i, 3);
    // if (ppn)
    //   m.pdpt_ppn = ppn;
    // else
    // {
    m.pdpt_ppn = PageManager::instance()->allocPPN();
    IPT->addRef(m.pdpt_ppn, my_proc, 0, 0, 3);
    //}
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pdpt_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pdpt_ppn);
    insert<PageMapLevel4Entry>((pointer) m.pml4, m.pml4i, m.pdpt_ppn, 1, 0, 1, 1);
  }

  if (m.pd_ppn == 0)
  {
    // ppn = cowPML<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti, 2);
    // if (ppn)
    //   m.pd_ppn = ppn;
    // else
    // {
    m.pd_ppn = PageManager::instance()->allocPPN();
    IPT->addRef(m.pd_ppn, my_proc, 0, 0, 2);
    //}
    
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pd_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pd_ppn);
    insert<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti, m.pd_ppn, 1, 0, 1, 1);
  }

  if (m.pt_ppn == 0)
  {
    // ppn = cowPML<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi, 1);
    // if (ppn)
    // {
    //   m.pt_ppn = ppn;
    // }
    // else
    // {
    m.pt_ppn = PageManager::instance()->allocPPN();
    IPT->addRef(m.pt_ppn, my_proc, 0, 0, 1);
    // }
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pt_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pt_ppn);
    insert<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi, m.pt_ppn, 1, 0, 1, 1);
  }

  if (m.page_ppn == 0)
  {
    bool worked = insert<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti, physical_page, 0, 0, user_access, 1);
    if (worked)
      IPT->addRef(physical_page, my_proc, virtual_page);
    
    unlockArchMemory();
    IPT->unlockIPT();
    return worked;
  }
  unlockArchMemory();
  IPT->unlockIPT();
  return false;
}

ArchMemory::~ArchMemory()
{
  debug(X_ARCHMEM, "~ArchMemory() called. will now lock\n archmem, ");
  InvertedPageTable* IPT = InvertedPageTable::instance();
  
  if (!IPT->checkIPT())
    IPT->lockIPT();
  
  lockArchMemory();
  int ret;
  assert(currentThread->kernel_registers_->cr3 != page_map_level_4_ * PAGE_SIZE && "thread deletes its own arch memory");

  PageMapLevel4Entry* pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  for (uint64 pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++) // free only lower half
  {
    if (pml4[pml4i].present)
    {
      PageDirPointerTableEntry* pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[pml4i].page_ppn);
      for (uint64 pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if (pdpt[pdpti].pd.present)
        {
          assert(pdpt[pdpti].pd.size == 0);
          PageDirEntry* pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (uint64 pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if (pd[pdi].pt.present)
            {
              assert(pd[pdi].pt.size == 0);
              PageTableEntry* pt = (PageTableEntry*) getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (uint64 pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if (pt[pti].present)
                {
                  size_t ppn = pt[pti].page_ppn;
                  size_t vpn = (pml4i << 39) + (pdpti << 30) + (pdi << 21) + (pti << 12);
                  vpn = vpn >> 12;

                  ret = IPT->deleteRef(ppn, my_proc, vpn);
                  if(ret == 0)
                  {
                    debug(X_USERPROCESS, "[%ld] ~ArchMemory(): will call freePPN(ppn = %lx)\n", my_proc->getPID(), ppn);
                    PageManager::instance()->freePPN(ppn);
                  }
                  else if(ret == (int) WAS_LAST)
                  {
                    IPT->deleteRef(ppn, my_proc, vpn);
                    PageManager::instance()->freePPN(ppn);
                  }
                  else
                  {
                    debug(X_USERTHREAD, "[%ld] ~unmapPage(): COULD NOT CALL FREE freePPN(ppn = %lx)\n", currentThread->getTID(), ppn);
                  //  pm->unlockIPT();
                  }
                  //not sure if i can do thiss.... (I really dont wanna cow everything just for deleting right after)
                  //pt[pti].present = 0;
                }
              }
              ret = IPT->deleteRef(pd[pdi].pt.page_ppn, my_proc, 0, 1);
              if (ret == 0)
              {
                PageManager::instance()->freePPN(pd[pdi].pt.page_ppn);
              }
              else if (ret == (int) WAS_LAST)
              {
                ret = IPT->deleteRef(pd[pdi].pt.page_ppn, my_proc, 0, 1);
                PageManager::instance()->freePPN(pd[pdi].pt.page_ppn);
              }
              //pd[pdi].pt.present = 0;
              //PageManager::instance()->freePPN(pd[pdi].pt.page_ppn);
            }
          }
          ret = IPT->deleteRef(pdpt[pdpti].pd.page_ppn, my_proc, 0, 2);
          if (ret == 0)
          {
            PageManager::instance()->freePPN(pdpt[pdpti].pd.page_ppn);
          }
          else if (ret == (int) WAS_LAST)
          {
            ret = IPT->deleteRef(pdpt[pdpti].pd.page_ppn, my_proc, 0, 2);
            PageManager::instance()->freePPN(pdpt[pdpti].pd.page_ppn);
          }
          // pdpt[pdpti].pd.present = 0;
          // PageManager::instance()->freePPN(pdpt[pdpti].pd.page_ppn);
        }
      }
      ret = IPT->deleteRef(pml4[pml4i].page_ppn, my_proc, 0, 3);
      if (ret == 0)
      {
        PageManager::instance()->freePPN(pml4[pml4i].page_ppn);
      }
      else if (ret == (int) WAS_LAST)
      {
        ret = IPT->deleteRef(pml4[pml4i].page_ppn, my_proc, 0, 3);
        PageManager::instance()->freePPN(pml4[pml4i].page_ppn);
      }
      // pml4[pml4i].present = 0;
      // PageManager::instance()->freePPN(pml4[pml4i].page_ppn);
    }
  }
  PageManager::instance()->freePPN(page_map_level_4_);
  unlockArchMemory();
  IPT->unlockIPT();

}

pointer ArchMemory::checkAddressValid(uint64 vaddress_to_check)
{
  ArchMemoryMapping m = resolveMapping(page_map_level_4_, vaddress_to_check / PAGE_SIZE);
  if (m.page != 0)
  {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> true\n", page_map_level_4_, vaddress_to_check);
    return m.page | (vaddress_to_check % m.page_size);
  }
  else
  {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> false\n", page_map_level_4_, vaddress_to_check);
    return 0;
  }
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 vpage)
{
  return resolveMapping(page_map_level_4_, vpage);
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 pml4, uint64 vpage)
{
  ArchMemoryMapping m;

  m.pti = vpage;
  m.pdi = m.pti / PAGE_TABLE_ENTRIES;
  m.pdpti = m.pdi / PAGE_DIR_ENTRIES;
  m.pml4i = m.pdpti / PAGE_DIR_POINTER_TABLE_ENTRIES;

  m.pti %= PAGE_TABLE_ENTRIES;
  m.pdi %= PAGE_DIR_ENTRIES;
  m.pdpti %= PAGE_DIR_POINTER_TABLE_ENTRIES;
  m.pml4i %= PAGE_MAP_LEVEL_4_ENTRIES;

  assert(pml4 < PageManager::instance()->getTotalNumPages());
  m.pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(pml4);
  m.pdpt = 0;
  m.pd = 0;
  m.pt = 0;
  m.page = 0;
  m.pml4_ppn = pml4;
  m.pdpt_ppn = 0;
  m.pd_ppn = 0;
  m.pt_ppn = 0;
  m.page_ppn = 0;
  m.page_size = 0;
  if (m.pml4[m.pml4i].present)
  {
    m.pdpt_ppn = m.pml4[m.pml4i].page_ppn;
    m.pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(m.pml4[m.pml4i].page_ppn);
    if (m.pdpt[m.pdpti].pd.present && !m.pdpt[m.pdpti].pd.size) // 1gb page ?
    {
      m.pd_ppn = m.pdpt[m.pdpti].pd.page_ppn;
      if (m.pd_ppn > PageManager::instance()->getTotalNumPages())
      {
        debug(A_MEMORY, "%zx\n", m.pd_ppn);
      }
      assert(m.pd_ppn < PageManager::instance()->getTotalNumPages());
      m.pd = (PageDirEntry*) getIdentAddressOfPPN(m.pdpt[m.pdpti].pd.page_ppn);
      if (m.pd[m.pdi].pt.present && !m.pd[m.pdi].pt.size) // 2mb page ?
      {
        m.pt_ppn = m.pd[m.pdi].pt.page_ppn;
        assert(m.pt_ppn < PageManager::instance()->getTotalNumPages());
        m.pt = (PageTableEntry*) getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
        if (m.pt[m.pti].present)
        {
          m.page = getIdentAddressOfPPN(m.pt[m.pti].page_ppn);
          m.page_ppn = m.pt[m.pti].page_ppn;
          //debug(X_ARCHMEM, "m.page_ppn : %lx\nTotalNumPages : %lx\n", m.page_ppn, PageManager::instance()->getTotalNumPages());
          assert(m.page_ppn < PageManager::instance()->getTotalNumPages());
          m.page_size = PAGE_SIZE;
        }
      }
      else if (m.pd[m.pdi].page.present)
      {
        m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES;
        m.page_ppn = m.pd[m.pdi].page.page_ppn;
        m.page = getIdentAddressOfPPN(m.pd[m.pdi].page.page_ppn);
      }
    }
    else if (m.pdpt[m.pdpti].page.present)
    {
      m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES;
      m.page_ppn = m.pdpt[m.pdpti].page.page_ppn;
      assert(m.page_ppn < PageManager::instance()->getTotalNumPages());
      m.page = getIdentAddressOfPPN(m.pdpt[m.pdpti].page.page_ppn);
    }
  }
  return m;
}

size_t ArchMemory::get_PPN_Of_VPN_In_KernelMapping(size_t virtual_page, size_t *physical_page,
                                                   size_t *physical_pte_page)
{
  ArchMemoryMapping m = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                       virtual_page);
  if (physical_page)
    *physical_page = m.page_ppn;
  if (physical_pte_page)
    *physical_pte_page = m.pt_ppn;
  return m.page_size;
}

void ArchMemory::mapKernelPage(size_t virtual_page, size_t physical_page)
{
  ArchMemoryMapping mapping = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                             virtual_page);
  PageMapLevel4Entry* pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt = (PageTableEntry*) getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(!pt[mapping.pti].present);
  pt[mapping.pti].writeable = 1;
  pt[mapping.pti].page_ppn = physical_page;
  pt[mapping.pti].present = 1;
  asm volatile ("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

void ArchMemory::unmapKernelPage(size_t virtual_page)
{
  ArchMemoryMapping mapping = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                             virtual_page);
  PageMapLevel4Entry* pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt = (PageTableEntry*) getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(pt[mapping.pti].present);
  pt[mapping.pti].present = 0;
  pt[mapping.pti].writeable = 0;
  PageManager::instance()->freePPN(pt[mapping.pti].page_ppn);
  asm volatile ("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

uint64 ArchMemory::getRootOfPagingStructure()
{
  return page_map_level_4_;
}

PageMapLevel4Entry* ArchMemory::getRootOfKernelPagingStructure()
{
  return kernel_page_map_level_4;
}

void ArchMemory::setCowToArchmemPages(ArchMemory &destination, UserProcess* child_proc)
{
  //UserProcess* parent = currentUserThread->getProcess();
  InvertedPageTable* IPT = InvertedPageTable::instance();
  if (!IPT->checkIPT())
    IPT->lockIPT();
  
  ustl::map<UserProcess*, size_t> procs;
  procs.emplace(my_proc, 0);
  procs.emplace(child_proc, 0);
  ProcessRegistry::instance()->lockMultArchmem(procs);
  IPTFlags* flags;
  
  PageMapLevel4Entry *pml4_src  = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  PageMapLevel4Entry *pml4_dest = (PageMapLevel4Entry*) getIdentAddressOfPPN(destination.page_map_level_4_);
  memcpy((void*) pml4_dest, (void*) pml4_src, PAGE_SIZE);
  for(size_t pml4i = 0; pml4i < (PAGE_MAP_LEVEL_4_ENTRIES/2); pml4i++)
  {
    if(pml4_src[pml4i].present)
    {
      //checkfor cow
      if (!pml4_src[pml4i].cow)
      {
        pml4_src[pml4i].cow = 1;
        pml4_src[pml4i].writeable = 0;
      }
      
      pml4_dest[pml4i] = pml4_src[pml4i];
      IPT->addRef(pml4_src[pml4i].page_ppn, child_proc, 0, 0, 3);
      flags = IPT->getFlags(pml4_src[pml4i].page_ppn);
      if(!flags->cow) 
        flags->cow = true;
     // pml4_dest[pml4i].page_ppn = PageManager::instance()->allocPPN();
      debug(X_ARCHMEM, "setCowToArchmemPages(): set cow to pdpt ppn = %lx\n",  (size_t)pml4_dest[pml4i].page_ppn);
      PageDirPointerTableEntry *pdpt_src  = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4_src[pml4i].page_ppn);
      
      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt_src[pdpti].pd.present)
        {
          if(!pdpt_src[pdpti].pd.cow)
          {
            pdpt_src[pdpti].pd.cow = 1;
            pdpt_src[pdpti].pd.writeable = 0;
          }
          IPT->addRef(pdpt_src[pdpti].pd.page_ppn, child_proc, 0, 0, 2);
          flags = IPT->getFlags(pdpt_src[pdpti].pd.page_ppn);
          if(!flags->cow) 
            flags->cow = true;
          //resolve cowwww

        // debug(X_ARCHMEM, "setCowToArchmemPages(): pdpt_dest[pdpti].pd.page_ppn = %lx\n", (size_t)pdpt_dest[pdpti].pd.page_ppn);
          PageDirEntry* pd_src  = (PageDirEntry*) getIdentAddressOfPPN(pdpt_src[pdpti].pd.page_ppn);
          // PageDirEntry* pd_dest = (PageDirEntry*) getIdentAddressOfPPN(pdpt_dest[pdpti].pd.page_ppn);
          // memcpy((void*) pd_dest, (void*) pd_src, PAGE_SIZE);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd_src[pdi].pt.present)
            {
              //pd_dest[pdi].pt.page_ppn = PageManager::instance()->allocPPN();
              if(!pd_src[pdi].pt.cow)
              {
                pd_src[pdi].pt.cow = 1;
                pd_src[pdi].pt.writeable = 0;
              }
              IPT->addRef(pd_src[pdi].pt.page_ppn, child_proc, 0, 0, 1);
              flags = IPT->getFlags(pd_src[pdi].pt.page_ppn);
              if(!flags->cow) 
                flags->cow = true;
            //  debug(X_ARCHMEM, "setCowToArchmemPages(): pd_dest[pdi].pt.page_ppn = %lx\n", (size_t)pd_dest[pdi].pt.page_ppn);
              //pd_src[pdi]
              PageTableEntry* pt_src  = (PageTableEntry*) getIdentAddressOfPPN(pd_src[pdi].pt.page_ppn);
              //PageTableEntry* pt_dest = (PageTableEntry*) getIdentAddressOfPPN(pd_dest[pdi].pt.page_ppn);
              //memcpy((void*) pt_dest, (void*) pt_src, PAGE_SIZE);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
               // PageManager::instance()->lockIPT();
                if(pt_src[pti].present)
                {
                  if(!pt_src[pti].cow)
                  {
                    pt_src[pti].cow = 1;
                    pt_src[pti].writeable = 0;
                  }
             
                  size_t vpn = (pml4i << 39) + (pdpti << 30) + (pdi << 21) + (pti << 12);
                  vpn = vpn >> 12;
                  debug(X_USERPROCESS, "cowing vpn: %lx\n", vpn);
                  IPT->addRef(pt_src[pti].page_ppn, child_proc, vpn);
                  flags = IPT->getFlags(pt_src[pti].page_ppn);
                  if(!flags->cow) 
                    flags->cow = true;
                }
               // PageManager::instance()->unlockIPT();
              }
            }
          }
        }
      }
    }
  }
  ProcessRegistry::instance()->unlockMultArchmem(procs);
  IPT->unlockIPT();
}

size_t ArchMemory::allocDestAndCopySrc(size_t ppn_src)
{
  size_t ppn_dest = PageManager::instance()->allocPPN();
  size_t vaddr_dest = ArchMemory::getIdentAddressOfPPN(ppn_dest);
  size_t vaddr_src  = ArchMemory::getIdentAddressOfPPN(ppn_src);
  //PageTableEntry* pt_src  = (PageTableEntry*) getIdentAddressOfPPN(pd_src[pdi].pt.page_ppn);
  memcpy((void*)vaddr_dest, (void*)vaddr_src, PAGE_SIZE);
  return ppn_dest;
}

template <typename T> 
size_t ArchMemory::cowPML(pointer entrypt,  size_t index, size_t level, size_t vpn)
{
  int ret;
  T* entry = (T*) entrypt;
  InvertedPageTable* IPT = InvertedPageTable::instance();

  if (entry[index].cow && entry[index].present)
  {
    ret = IPT->deleteRef(entry[index].page_ppn, my_proc, vpn, level);
    if (ret == (int) WAS_LAST)
    {
      entry[index].cow = 0;
      entry[index].writeable = 1;
      //next_PML = entry[index].page_ppn;
    }
    else if (ret > 0)
    {
      entry[index].present = 0;
      entry[index].page_ppn = allocDestAndCopySrc(entry[index].page_ppn);
      entry[index].cow = 0;
      entry[index].writeable = 1;
      IPT->addRef(entry[index].page_ppn, my_proc, vpn, 0, level);
      entry[index].present = 1;
    }
    else
    {
      debug(X_USERPROCESS, "Cow returned me %d\n", ret);
      assert(false && "something went wrong with the cow");
    }
  }
  else if (!entry[index].present)
  {
    return 0;
  } 
  return entry[index].page_ppn;
}


//lock IPT and archmem from outside
// do with template ...
bool ArchMemory::checkforPMLCow(size_t vpn, bool unmap)
{
  ArchMemoryMapping m = resolveMapping(vpn);
  m.pdpt_ppn = cowPML<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn), m.pml4i, 3);
  if(!m.pdpt_ppn)
    return false;
  m.pd_ppn = cowPML<PageDirPointerTablePageDirEntry> (getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti, 2);
  if(!m.pd_ppn)
    return false;
  m.pt_ppn = cowPML<PageDirPageTableEntry> (getIdentAddressOfPPN(m.pd_ppn), m.pdi, 1); 
  if(!m.pt_ppn)
    return false;
  if (!unmap)
  {
    m.page_ppn = cowPML<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti, 0, vpn);
    if (!m.page_ppn)
      return false;
  }
  return true;
}

ustl::pair<size_t, size_t> ArchMemory::cowUntil(size_t ppn, size_t level)
{
  if (level < 1 || level > 3)
  {
    debug(MULTICOW, "Wrong level!!\n");
    return ustl::make_pair(0,0);
  }
  debug(MULTICOW, "now starting to cow until %ld for proc [%ld]", level, my_proc->getPID());
  assert(page_map_level_4_);
  size_t page;
  PageMapLevel4Entry* pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if (level == 3)
    {
      if (pml4[pml4i].present && pml4[pml4i].page_ppn == ppn)
        return ustl::make_pair(page_map_level_4_, pml4i);
      continue;
    } 
    if (!pml4[pml4i].present)
      continue;
    
    PageDirPointerTableEntry* pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[pml4i].page_ppn);
    for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
    {
      if (level == 2)
      {
        if (pdpt[pdpti].pd.present && pdpt[pdpti].pd.page_ppn == ppn)
        {
          pml4[pml4i].page_ppn = cowPML<PageMapLevel4Entry>(getIdentAddressOfPPN(page_map_level_4_), pml4i, 3);
          assert(pml4[pml4i].page_ppn != 0 && "this should be present!");
          page = pml4[pml4i].page_ppn;
          debug(MULTICOW, "Hit! found the page! cowing pdpt resulted in %lx for pdpt\n", page);
          //pdpt = getIdentAddressOfPPN(pml4[pml4i].page_ppn);
          return ustl::make_pair(page, pdpti);
        }
        continue;
      }
      if (!pdpt[pdpti].pd.present)
        continue;
      
      PageDirEntry* pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
      for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
      {
        if (pd[pdi].pt.present && (pd[pdi].pt.page_ppn == ppn))
        {
          pml4[pml4i].page_ppn = cowPML<PageMapLevel4Entry>(getIdentAddressOfPPN(page_map_level_4_), pml4i, 3);
          assert(pml4[pml4i].page_ppn != 0 && "this should be present!");
          pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[pml4i].page_ppn);
          pdpt[pdpti].pd.page_ppn = cowPML<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(pml4[pml4i].page_ppn), pdpti, 2);
          assert(pdpt[pdpti].pd.page_ppn != 0 && "should be present aswell");
          page = pdpt[pdpti].pd.page_ppn;
          debug(MULTICOW, "cowing pd resulted in %lx for pdpt and %lx for pd\n", pml4[pml4i].page_ppn, page);
          return ustl::make_pair(page, pdi);
        }
      }
    }
  }
  debug(MULTICOW, "Did NOT find what you re looking for(%lx on %ld)\n", page, level);
  return ustl::make_pair(0,0);
}




