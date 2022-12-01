#include "ArchMemory.h"
#include "ArchInterrupts.h"
#include "kprintf.h"
#include "assert.h"
#include "PageManager.h"
#include "kstring.h"
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
  PageManager* pm = PageManager::instance();
  //lockArchMemory();
  ArchMemoryMapping m = resolveMapping(virtual_page);

  assert(m.page_ppn != 0 && m.page_size == PAGE_SIZE && m.pt[m.pti].present);

  PageTableEntry* pt_ident  = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
  // free if counter not in map or after descresing counter = 0.
  size_t ppn = m.pt[m.pti].page_ppn;
  
  pm->lockCowCnt();
  if(pm->deleteRef(ppn, my_proc, false) == 0)
  {
    debug(X_USERTHREAD, "[%ld] ~unmapPage(): will call freePPN(ppn = %lx)\n", currentThread->getTID(), ppn);
    PageManager::instance()->freePPN(ppn);
    pm->unlockCowCnt();
  }
  else
  {
    debug(X_USERTHREAD, "[%ld] ~unmapPage(): COULD NOT CALL FREE freePPN(ppn = %lx)\n", currentThread->getTID(), ppn);
    pm->unlockCowCnt();
  }
  pt_ident[m.pti].present = 0;

  
  //pm->unlockCowCnt();

  ((uint64*)m.pt)[m.pti] = 0; // for easier debugging
  bool empty = checkAndRemove<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti);
  if (empty)
  {
    empty = checkAndRemove<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pt_ppn = %zx)\n", virtual_page, m.pt_ppn);
    PageManager::instance()->freePPN(m.pt_ppn);
  }
  if (empty)
  {
    empty = checkAndRemove<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pd_ppn = %zx)\n", virtual_page, m.pd_ppn);
    PageManager::instance()->freePPN(m.pd_ppn);
  }
  if (empty)
  {
    empty = checkAndRemove<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn), m.pml4i);
    debug(X_ARCHMEM, "unmapPage(virtual_page = %zx) calling freePPN(m.pdpt_ppn = %zx)\n", virtual_page, m.pdpt_ppn);
    PageManager::instance()->freePPN(m.pdpt_ppn);
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
  if (!checkArchMemory(currentThread))
    lockArchMemory();
  
  debug(A_MEMORY, "%zx %zx %zx %zx\n", page_map_level_4_, virtual_page, physical_page, user_access);
  ArchMemoryMapping m = resolveMapping(page_map_level_4_, virtual_page);
  assert((m.page_size == 0) || (m.page_size == PAGE_SIZE));

  if (m.pdpt_ppn == 0)
  {
    m.pdpt_ppn = PageManager::instance()->allocPPN();
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pdpt_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pdpt_ppn);
    insert<PageMapLevel4Entry>((pointer) m.pml4, m.pml4i, m.pdpt_ppn, 1, 0, 1, 1);
  }

  if (m.pd_ppn == 0)
  {
    m.pd_ppn = PageManager::instance()->allocPPN();
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pd_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pd_ppn);
    insert<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti, m.pd_ppn, 1, 0, 1, 1);
  }

  if (m.pt_ppn == 0)
  {
    m.pt_ppn = PageManager::instance()->allocPPN();
    debug(X_ARCHMEM, "mapPage(virtual_page = %zx, physical_page = %zx, user_access = %zx) m.pt_ppn was zero. set to ppn = %zx. inserting\n", virtual_page, physical_page, user_access, m.pt_ppn);
    insert<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi, m.pt_ppn, 1, 0, 1, 1);
  }

  if (m.page_ppn == 0)
  {
    unlockArchMemory();
    return insert<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti, physical_page, 0, 0, user_access, 1);
  }
  unlockArchMemory();
  return false;
}

ArchMemory::~ArchMemory()
{
  debug(X_ARCHMEM, "~ArchMemory() called. will now lock\n archmem, ");
  lockArchMemory();
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
                  PageManager* pm = PageManager::instance();
                  pm->lockCowCnt();
                  size_t ppn = pt[pti].page_ppn;
                  if(pm->deleteRef(ppn, my_proc, false) == 0)
                  {
                    debug(X_USERPROCESS, "[%ld] ~ArchMemory(): will call freePPN(ppn = %lx)\n", my_proc->getPID(), ppn);
                    pm->freePPN(ppn);
                    pt[pti].present = 0;
                  }
                  pm->unlockCowCnt();
                }
              }
              pd[pdi].pt.present = 0;
              PageManager::instance()->freePPN(pd[pdi].pt.page_ppn);
            }
          }
          pdpt[pdpti].pd.present = 0;
          PageManager::instance()->freePPN(pdpt[pdpti].pd.page_ppn);
        }
      }
      pml4[pml4i].present = 0;
      PageManager::instance()->freePPN(pml4[pml4i].page_ppn);
    }
  }
  PageManager::instance()->freePPN(page_map_level_4_);
  unlockArchMemory();
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
  UserProcess* parent = currentUserThread->getProcess();
  if(!checkArchMemory(currentThread))
    lockArchMemory();
  if(!destination.checkArchMemory(currentThread))
    destination.lockArchMemory();
  
  PageMapLevel4Entry *pml4_src  = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  PageMapLevel4Entry *pml4_dest = (PageMapLevel4Entry*) getIdentAddressOfPPN(destination.page_map_level_4_);
  memcpy((void*) pml4_dest, (void*) pml4_src, PAGE_SIZE);
  for(size_t pml4i = 0; pml4i < (PAGE_MAP_LEVEL_4_ENTRIES/2); pml4i++)
  {
    if(pml4_src[pml4i].present)
    {
      //pml4_dest[pml4i].page_ppn = PageManager::instance()->allocPPN();
      debug(X_ARCHMEM, "setCowToArchmemPages(): pml4_dest[pml4i].page_ppn = %lx\n",  (size_t)pml4_dest[pml4i].page_ppn);
      PageDirPointerTableEntry *pdpt_src  = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4_src[pml4i].page_ppn);
      PageDirPointerTableEntry *pdpt_dest = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4_dest[pml4i].page_ppn);
      
      pml4_src[pml4i].cow = 1;
      pml4_src[pml4i].writeable = 0;
      pml4_dest[pml4i] = pml4_src[pml4i];
      
      
      //memcpy((void*) pdpt_dest, (void*) pdpt_src, PAGE_SIZE);
      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt_src[pdpti].pd.present)
        {
          pdpt_src[pdpti].pd.cow = 1;
          pdpt_src[pdpti].pd.writeable = 0;

          pdpt_dest[pdpti].pd = pdpt_src[pdpti].pd;//= PageManager::instance()->allocPPN();
          debug(X_ARCHMEM, "setCowToArchmemPages(): pdpt_dest[pdpti].pd.page_ppn = %lx\n", (size_t)pdpt_dest[pdpti].pd.page_ppn);
          PageDirEntry* pd_src  = (PageDirEntry*) getIdentAddressOfPPN(pdpt_src[pdpti].pd.page_ppn);
          PageDirEntry* pd_dest = (PageDirEntry*) getIdentAddressOfPPN(pdpt_dest[pdpti].pd.page_ppn);
          //memcpy((void*) pd_dest, (void*) pd_src, PAGE_SIZE);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd_src[pdi].pt.present)
            {
              pd_src[pdi].pt.cow = 1;
              pd_src[pdi].pt.writeable = 0;

              pd_dest[pdi].pt = pd_src[pdi].pt; // = PageManager::instance()->allocPPN();
              debug(X_ARCHMEM, "setCowToArchmemPages(): pd_dest[pdi].pt.page_ppn = %lx\n", (size_t)pd_dest[pdi].pt.page_ppn);
              PageTableEntry* pt_src  = (PageTableEntry*) getIdentAddressOfPPN(pd_src[pdi].pt.page_ppn);
              PageTableEntry* pt_dest = (PageTableEntry*) getIdentAddressOfPPN(pd_dest[pdi].pt.page_ppn);
              //memcpy((void*) pt_dest, (void*) pt_src, PAGE_SIZE);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                PageManager::instance()->lockCowCnt();
                if(pt_src[pti].present)
                {
                  pt_src[pti].cow = 1;
                  pt_src[pti].writeable = 0;
                  // pt_dest[pti].cow = 1;
                  // pt_dest[pti].writeable = 0;
                  // pt_dest[pti].page_ppn = pt_src[pti].page_ppn;
                  pt_dest[pti] = pt_src[pti];
                  //debug(X_USERPROCESS, "adding proc %ld\n", parent->getPID());
                  PageManager::instance()->addRef(pt_src[pti].page_ppn, parent);
                  //debug(X_USERPROCESS, "adding proc %ld\n", child_proc->getPID());
                  PageManager::instance()->addRef(pt_dest[pti].page_ppn, child_proc);
                }
                PageManager::instance()->unlockCowCnt();
              }
            }
          }
        }
      }
    }
  }
  destination.unlockArchMemory();
  unlockArchMemory();
}

/*
void ArchMemory::copyOnWrite(size_t address)
{
  debug(A_MEMORY,"Entering copy on write function");
  arch_memory_lock_.acquire();
  size_t virtual_page = address/PAGE_SIZE;

  ArchMemoryMapping m = resolveMapping(virtual_page);

  size_t used_page = m.pt[m.pti].page_ppn;

  assert((m.pt[m.pti].cow || m.pt[m.pti].writeable) && "COW 1 & WRITABLE 1 ?!?!?!!?");
  cow_cnt_lock_.acquire();

  if(cow_counter_.find(used_page) == cow_counter_.end())
  {
      debug(A_MEMORY,"Page %ld not in the cow_counter even tho flags are set!\n", used_page);
      cow_cnt_lock_.release();
      arch_memory_lock_.release();
      return;
  }
  // this is the only case cow needs to be cow 1 and writable 0
  if(cow_counter_.at(used_page) > 1)
  {
    if  (m.pt[m.pti].cow && !m.pt[m.pti].writeable)
    {
        m.pt[m.pti].page_ppn = PageManager::instance()->allocPPN();
        void* page_curr = (void*)getIdentAddressOfPPN(used_page);
        void* page_dest = (void*)getIdentAddressOfPPN(m.pt[m.pti].page_ppn);
        memcpy(page_dest, page_curr, PAGE_SIZE);
        debug(A_MEMORY,"Copied page in COW from %lx to %lx!\n", used_page,(size_t) m.pt[m.pti].page_ppn);
        cow_counter_.at(used_page)--;
        m.pt[m.pti].cow = 0;
        m.pt[m.pti].writeable = 1;
    }
  }

  if(cow_counter_.at(used_page) == 1)
  {
    m.pt[m.pti].cow = 0;
    m.pt[m.pti].writeable = 1;
    cow_counter_.erase(used_page);
  }
  cow_cnt_lock_.release();
  arch_memory_lock_.release();
}
*/

size_t ArchMemory::allocDestAndCopySrc(size_t ppn_src)
{
  size_t ppn_dest = PageManager::instance()->allocPPN();
  size_t vaddr_dest = ArchMemory::getIdentAddressOfPPN(ppn_dest);
  size_t vaddr_src  = ArchMemory::getIdentAddressOfPPN(ppn_src);
  //PageTableEntry* pt_src  = (PageTableEntry*) getIdentAddressOfPPN(pd_src[pdi].pt.page_ppn);
  memcpy((void*)vaddr_dest, (void*)vaddr_src, PAGE_SIZE);
  return ppn_dest;
}