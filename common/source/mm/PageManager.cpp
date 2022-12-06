#include "PageManager.h"
#include "new.h"
#include "offsets.h"
#include "paging-definitions.h"
#include "ArchCommon.h"
#include "ArchMemory.h"
#include "debug_bochs.h"
#include "kprintf.h"
#include "kstring.h"
#include "Scheduler.h"
#include "ArchInterrupts.h"
#include "KernelMemoryManager.h"
#include "assert.h"
#include "ProcessRegistry.h"
#include "Bitmap.h"

PageManager pm;

PageManager* PageManager::instance_ = 0;

PageManager* PageManager::instance()
{
  if (unlikely(!instance_))
    new (&pm) PageManager();
  return instance_;
}

PageManager::PageManager() : lock_("PageManager::lock_")
{
  assert(instance_ == 0);
  instance_ = this;
  assert(KernelMemoryManager::instance_ == 0);
  number_of_pages_ = 0;
  lowest_unreserved_page_ = 0;

  size_t num_mmaps = ArchCommon::getNumUseableMemoryRegions();

  size_t highest_address = 0, used_pages = 0;

  //Determine Amount of RAM
  for (size_t i = 0; i < num_mmaps; ++i)
  {
    pointer start_address = 0, end_address = 0, type = 0;
    ArchCommon::getUsableMemoryRegion(i, start_address, end_address, type);
    debug(PM, "Ctor: memory region from physical %zx to %zx (%zu bytes) of type %zd\n",
          start_address, end_address, end_address - start_address, type);

    if (type == 1)
      highest_address = Max(highest_address, end_address & 0x7FFFFFFF);
  }

  number_of_pages_ = highest_address / PAGE_SIZE;

  size_t boot_bitmap_size = Min(4096 * 8 * 2, number_of_pages_);
  uint8 page_usage_table[BITMAP_BYTE_COUNT(boot_bitmap_size)];
  used_pages = boot_bitmap_size;
  memset(page_usage_table,0xFF,BITMAP_BYTE_COUNT(boot_bitmap_size));

  //mark as free, everything that might be useable
  for (size_t i = 0; i < num_mmaps; ++i)
  {
    pointer start_address = 0, end_address = 0, type = 0;
    ArchCommon::getUsableMemoryRegion(i, start_address, end_address, type);
    if (type != 1)
      continue;
    size_t start_page = start_address / PAGE_SIZE;
    size_t end_page = end_address / PAGE_SIZE;
    debug(PM, "Ctor: usable memory region: start_page: %zx, end_page: %zx, type: %zd\n", start_page, end_page, type);

    for (size_t k = Max(start_page, lowest_unreserved_page_); k < Min(end_page, number_of_pages_); ++k)
    {
      Bitmap::unsetBit(page_usage_table, used_pages, k);
    }
  }

  debug(PM, "Ctor: Marking pages used by the kernel as reserved\n");
  for (size_t i = ArchMemory::RESERVED_START; i < ArchMemory::RESERVED_END; ++i)
  {
    size_t physical_page = 0;
    size_t pte_page = 0;
    size_t this_page_size = ArchMemory::get_PPN_Of_VPN_In_KernelMapping(i, &physical_page, &pte_page);
    assert(this_page_size == 0 || this_page_size == PAGE_SIZE || this_page_size == PAGE_SIZE * PAGE_TABLE_ENTRIES);
    if (this_page_size > 0)
    {
      //our bitmap only knows 4k pages for now
      uint64 num_4kpages = this_page_size / PAGE_SIZE; //should be 1 on 4k pages and 1024 on 4m pages
      for (uint64 p = 0; p < num_4kpages; ++p)
      {
        if (physical_page * num_4kpages + p < number_of_pages_)
          Bitmap::setBit(page_usage_table, used_pages, physical_page * num_4kpages + p);
      }
      i += (num_4kpages - 1); //+0 in most cases

      if (num_4kpages == 1 && i % 1024 == 0 && pte_page < number_of_pages_)
        Bitmap::setBit(page_usage_table, used_pages, pte_page);
    }
  }

  debug(PM, "Ctor: Marking GRUB loaded modules as reserved\n");
  //LastbutNotLeast: Mark Modules loaded by GRUB as reserved (i.e. pseudofs, etc)
  for (size_t i = 0; i < ArchCommon::getNumModules(); ++i)
  {
    size_t start_page = (ArchCommon::getModuleStartAddress(i) & 0x7FFFFFFF) / PAGE_SIZE;
    size_t end_page = (ArchCommon::getModuleEndAddress(i) & 0x7FFFFFFF) / PAGE_SIZE;
    debug(PM, "Ctor: module: start_page: %zx, end_page: %zx\n", start_page, end_page);
    for (size_t k = Min(start_page, number_of_pages_); k <= Min(end_page, number_of_pages_ - 1); ++k)
    {
      Bitmap::setBit(page_usage_table, used_pages, k);
      if (ArchMemory::get_PPN_Of_VPN_In_KernelMapping(PHYSICAL_TO_VIRTUAL_OFFSET / PAGE_SIZE + k, 0, 0) == 0)
        ArchMemory::mapKernelPage(PHYSICAL_TO_VIRTUAL_OFFSET / PAGE_SIZE + k,k);
    }
  }

  size_t num_pages_for_bitmap = (number_of_pages_ / 8) / PAGE_SIZE + 1;
  assert(used_pages < number_of_pages_/2 && "No space for kernel heap!");

  HEAP_PAGES = number_of_pages_/2 - used_pages;
  if (HEAP_PAGES > 1024)
    HEAP_PAGES = 1024 + (HEAP_PAGES - Min(HEAP_PAGES,1024))/8;

  size_t start_vpn = ArchCommon::getFreeKernelMemoryStart() / PAGE_SIZE;
  size_t free_page = 0;
  size_t temp_page_size = 0;
  size_t num_reserved_heap_pages = 0;
  for (num_reserved_heap_pages = 0; num_reserved_heap_pages < num_pages_for_bitmap || temp_page_size != 0 ||
                                    num_reserved_heap_pages < ((DYNAMIC_KMM || (number_of_pages_ < 512)) ? 0 : HEAP_PAGES); ++num_reserved_heap_pages)
  {
    while (!Bitmap::setBit(page_usage_table, used_pages, free_page))
      free_page++;
    if ((temp_page_size = ArchMemory::get_PPN_Of_VPN_In_KernelMapping(start_vpn,0,0)) == 0)
      ArchMemory::mapKernelPage(start_vpn,free_page++);
    start_vpn++;
  }

  extern KernelMemoryManager kmm;
  new (&kmm) KernelMemoryManager(num_reserved_heap_pages,HEAP_PAGES);
  page_usage_table_ = new Bitmap(number_of_pages_);

  for (size_t i = 0; i < boot_bitmap_size; ++i)
  {
    if (Bitmap::getBit(page_usage_table,i))
      page_usage_table_->setBit(i);
  }

  debug(PM, "Ctor: find lowest unreserved page\n");
  for (size_t p = 0; p < number_of_pages_; ++p)
  {
    if (!page_usage_table_->getBit(p))
    {
      lowest_unreserved_page_ = p;
      break;
    }
  }
  debug(PM, "Ctor: Physical pages - free: %zu used: %zu total: %u\n", page_usage_table_->getNumFreeBits(),
        page_usage_table_->getNumBitsSet(), number_of_pages_);
  assert(lowest_unreserved_page_ < number_of_pages_);


  debug(PM, "Clearing free pages\n");
  for(size_t p = lowest_unreserved_page_; p < number_of_pages_; ++p)
  {
    if(!page_usage_table_->getBit(p))
    {
      memset((void*)ArchMemory::getIdentAddressOfPPN(p), 0xFF, PAGE_SIZE);
    }
  }

  KernelMemoryManager::pm_ready_ = 1;
}

uint32 PageManager::getTotalNumPages() const
{
  return number_of_pages_;
}

size_t PageManager::getNumFreePages() const
{
  return page_usage_table_->getNumFreeBits();
}

bool PageManager::reservePages(uint32 ppn, uint32 num)
{
  assert(lock_.heldBy() == currentThread);
  if (ppn < number_of_pages_ && !page_usage_table_->getBit(ppn))
  {
    if (num == 1 || reservePages(ppn + 1, num - 1))
    {
      page_usage_table_->setBit(ppn);
      return true;
    }
  }
  return false;
}

uint32 PageManager::allocPPN(uint32 page_size)
{
  uint32 p;
  uint32 found = 0;

  assert((page_size % PAGE_SIZE) == 0);

  lock_.acquire();

  for (p = lowest_unreserved_page_; !found && (p < number_of_pages_); ++p)
  {
    if ((p % (page_size / PAGE_SIZE)) != 0)
      continue;
    if (reservePages(p, page_size / PAGE_SIZE))
      found = p;
  }
  while ((lowest_unreserved_page_ < number_of_pages_) && page_usage_table_->getBit(lowest_unreserved_page_))
    ++lowest_unreserved_page_;

  lock_.release();

  if (found == 0)
  {
    assert(false && "PageManager::allocPPN: Out of memory / No more free physical pages");
  }

  const char* page_ident_addr = (const char*)ArchMemory::getIdentAddressOfPPN(found);
  const char* page_modified = (const char*)memnotchr(page_ident_addr, 0xFF, page_size);
  if(page_modified)
  {
    debug(PM, "Detected use-after-free for PPN %x at offset %zx\n", found, page_modified - page_ident_addr);
    assert(!page_modified && "Page modified after free");
  }

  memset((void*)ArchMemory::getIdentAddressOfPPN(found), 0, page_size);
  debug(X_PAGEMANAGER, "allocPPN() found page at %x\n", found);
  return found;
}

void PageManager::freePPN(uint32 page_number, uint32 page_size)
{
  assert((page_size % PAGE_SIZE) == 0);
  //debug(X_PAGEMANAGER, "freePPN(page_numer = %x, page_size = %d) entered.\n", page_number, page_size);

  // does this also set present to 0?
  memset((void*)ArchMemory::getIdentAddressOfPPN(page_number), 0xFF, page_size);

  lock_.acquire();
  if (page_number < lowest_unreserved_page_)
    lowest_unreserved_page_ = page_number;
  for (uint32 p = page_number; p < (page_number + page_size / PAGE_SIZE); ++p)
  {
    assert(page_usage_table_->getBit(p) && "Double free PPN");
    page_usage_table_->unsetBit(p);
  }
  lock_.release();
}

//----------------------------------------------------------------------------------cow start
// bool PageManager::isInCowCnt(size_t ppn)
// { 
//   assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE lockIPT()!!!!");
//   return cow_cnt_.find(ppn) != cow_cnt_.end(); 
// }

// size_t PageManager::getNrOfCows(size_t ppn)
// { 
//   assert(IPT_lock_.isHeldBy(currentThread) && "PLEASE lockIPT()!!!!");
//   assert(isInCowCnt(ppn) && "if you use this method pls check isInCowCnt(ppn) before");
//   debug(X_PAGEMANAGER, "getNrOfCows(%lx) found value %ld\n", ppn, cow_cnt_[ppn]);
//   return cow_cnt_[ppn]; 
// }


//LOCK ARCHMEM OUTSIDE!!
bool PageManager::checkForCow(size_t address)
{
  debug(X_PAGEFAULT, "checkForCow(%lx) entered. will now checkAddressValid()\n", address);
  // setup archmem and checkAddressValid()

  UserProcess* current_proc = currentUserThread->getProcess();
  ArchMemory* current_archmem = &current_proc->getLoader()->arch_memory_;
  size_t vpn = address/PAGE_SIZE;
  //size_t mutexflag = AWAKE_KS;
  
  
  
  //checks if we just have a load request... we dont want to lock ipt for that
  if (!current_archmem->checkArchMemory(currentThread))
    current_archmem->lockArchMemory();
 
  ArchMemoryMapping m = current_archmem->resolveMapping(vpn);
  if(!current_archmem->checkAddressValid(address))
  {
    debug(X_PAGEFAULT, "checkForCow(%lx) says checkAddressValid() failed. return false\n", address);
  //  unlockIPT();
    current_archmem->unlockArchMemory();
    return false;
  }

  if(!m.pt[m.pti].present)
  {
    debug(X_PAGEFAULT, "checkForCow(%lx) says !present. return false\n", address);
   // unlockIPT();
    current_archmem->unlockArchMemory();
    return false;
  }
  current_archmem->unlockArchMemory();

  InvertedPageTable* IPT = InvertedPageTable::instance();
  if (!IPT->checkIPT())
    IPT->lockIPT();
  
  current_archmem->lockArchMemory();
  
  //need to resolve again because sth might have changed in the meanttime.... IPT lock waiting can take looong
  m = current_archmem->resolveMapping(vpn);
  //0x7b68de0eb570
  //0x7b68de0fafb0

  if(!(m.pt[m.pti].cow && !m.pt[m.pti].writeable) || !(m.pt[m.pti].present))
  {
    debug(X_PAGEFAULT, "checkForCow(%lx) says not (cow =1 AND writable = 0). return false\n", address);
  //  unlockIPT();
    current_archmem->unlockArchMemory();
    IPT->unlockIPT();
    return false;
  }
  
  debug(X_PAGEFAULT, "seems like we re in cow\n");
  //this is for keeping correct locking convention
  
  size_t ppn = m.page_ppn;
  
  //need ident??
  PageTableEntry* pt_ident  = (PageTableEntry*) ArchMemory::getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
  debug(X_USERPROCESS, "my PageTable is at page %x/%x the page at %lx\n", m.pd[m.pdi].pt.page_ppn, m.pt->page_ppn, ppn);
  int ret = IPT->deleteRef(ppn, current_proc, vpn); 
  //bool dbg_gave = false;
  if (ret == -1)
  {
    IPT->unlockIPT();
    //lets leave this in here for now
    assert(false && "Something went wrong with deleting my ref!\n");
    return false;
  }
  if (ret == (int)WAS_LAST)
  {
    pt_ident[m.pti].cow = 0;
    pt_ident[m.pti].writeable = 1;
    //dbg_gave = true;
  }
  else if (ret == 0)
  {
    assert(false && "IPT didnt know this was a cow Page?? Hmmmm.... -.-\n");
  }
  else
  {
    pt_ident[m.pti].present = 0;
    pt_ident[m.pti].page_ppn = current_archmem->allocDestAndCopySrc(ppn);
    pt_ident[m.pti].cow = 0;
    pt_ident[m.pti].writeable = 1;
    pt_ident[m.pti].present = 1;
    InvertedPageTable::instance()->addRef(pt_ident[m.pti].page_ppn, current_proc, vpn);
  }

  // F*ck the userspace mutexes now ... they bring no points and only lead to problems...
  // UserThread* mut_change = 0;
  // if (address > END_OF_STACKS && address < USER_BREAK)
  // {
  //   currentUserProcess->lockThreadMutex();
  //   if((mut_change = currentUserThread->getProcess()->checkStackAdress(address)) != 0)
  //   {
  //     if(vpn == (mut_change->getStackInfo()->userstack_start_ / PAGE_SIZE))
  //     {
  //       debug(X_PAGEMANAGER, "seems like we re cowing THE FIRRST stackpage... of [%ld]time to change ident\n", mut_change->getTID());
  //       mutexflag = *mut_change->getStackInfo()->UserMutex;
  //       debug(X_PAGEMANAGER, "mutexflag is %s\n", (mutexflag == AWAKE_KS) ? "awake" : "zzzz");
  //       size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(pt_ident[m.pti].page_ppn);
  //       location += PAGE_SIZE - sizeof(size_t);
  //       size_t* dbg_flag  = mut_change->getStackInfo()->UserMutex;
  //       debug(X_USERTHREAD, "PREV my mutexflag is now at %p\n", dbg_flag);
  //       mut_change->setUserMutex((size_t*) location);
  //       dbg_flag = mut_change->getStackInfo()->UserMutex;
  //       debug(X_USERTHREAD, "AFTER my mutexflag is now at %p\n", dbg_flag);
  //       //debug(X_USERTHREAD, "AFTER my mutexflag is now at %p\n", currentUserThread->getStackInfo()->UserMutex);
  //       *mut_change->getStackInfo()->UserMutex = mutexflag;
  //      // mut_change->setUserMutex(USERMUTEX_INVALID);
  //     }
  //     else
  //       mut_change = 0;
  //   }
  //   currentUserProcess->unLockThreadMutex();  
  // }
  
  current_archmem->unlockArchMemory();
  IPT->unlockIPT();

  //current_archmem->unlockArchMemory();
  return true;
}


//----------------------------------------------------------------------------------cow end