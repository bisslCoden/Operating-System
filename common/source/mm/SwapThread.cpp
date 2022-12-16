#include "SwapThread.h"
#include "debug.h"
#include "PageReplacementAlgos.h"

SwapThread* SwapThread::instance_ = 0;

SwapThread* SwapThread::instance()
{
  if (unlikely(!instance_))
    instance_ = new SwapThread();
  return instance_;
}

SwapThread::SwapThread() :
  Thread(0, "SwapThread", KERNEL_THREAD, 0),
  lock_requests_swap_in_("SwapThread::lock_requests_swap_in_"),
  lock_requests_swap_out_("SwapThread::lock_requests_swap_out_"),
  lock_requests_swap_remove_entry_("SwapThread::lock_requests_swap_remove_entry_")
{
  swap_cnt_ = PageManager::instance()->getTotalNumPages() + SICHERHEITSABSTAND;
  // maybe get the IPT
  // maybe get the PRAs
  // both when implemented
}


// -----------------------------------------------------------------------------
//                           getting them requests
// -----------------------------------------------------------------------------

void SwapThread::requestSwapOutAndSleep(uint32* found_ptr)
{
  debug(SWAPREQUEST, "requestSwapOutAndSleep(found_ptr = %lx) by [%ld] will push and sleep until i got a page\n", (size_t)found_ptr, currentThread->getTID());
  assert(found_ptr && ((*found_ptr) == 0) && "wat?");

  // Swap
  if(currentThread == this)
  {
    //assert(currentThread == this && "KernelThread calling requestSwapOutAndSleep was not SwapThread.");
    debug(SWAPREQUEST, "SwapThread's allocPPN() sent a requestSwapOut() Better freePPN(swapOut())\n");
    for(int i = 0; i < SWAPTHREAD_LOAD; i++)
      PageManager::instance()->freePPN(swapOut());
    *found_ptr = swapOut();
    return;
  }

  // create request
  SwapOut request{};
  request.found_ptr = found_ptr;
  Condition request_cond(&lock_requests_swap_out_ , "SwapThread::SwapOut::cond_swap_out");
  request.cond_swap_out = &request_cond;

  // push request and sleep
  debug(SWAPREQUEST, "requestSwapOutAndSleep(found_ptr = %lx) by [%ld] schleepp now\n", (size_t)found_ptr, currentThread->getTID());
  lock_requests_swap_out_.acquire();
  requests_swap_out_.push_back(request);
  request.cond_swap_out->waitAndRelease();

  // awake again
  debug(SWAPREQUEST, "requestSwapOut() by TID [%ld] will continue with found_ptr = %lx\n", currentThread->getTID(), (size_t)found_ptr);
  assert(request.found_ptr && "requesSwapOut() woke up and found_ptr became 0!?!");
}


void SwapThread::requestSwapInAndSleep(uint32 swap_id)
{
  debug(SWAPREQUEST, "requestSwapInAndSleep(ppn = %x) by [%ld] will push and sleep now.\n", swap_id, currentThread->getTID());
  assert(currentThread != this && "ALAAAAAAAAAAAAAAAAAAAAAAAAAAAAARRRRRM I DONNT KNOW WHAT TO DOOOOO IN REQUEST SWAP IIIIINNNNNNN...???");

  // create request 
  SwapIn request{};
  request.swap_id = swap_id;
  Condition request_cond(&lock_requests_swap_in_, "SwapThread::SwapIn::cond_swap_in");
  request.cond_swap_in = &request_cond;

  // push request and sleep 
  lock_requests_swap_in_.acquire();
  requests_swap_in_.push_back(request);
  request.cond_swap_in->waitAndRelease();
  
  debug(SWAPREQUEST, "requestSwapInAndSleep(ppn = %x) by [%ld] woke up again\n", swap_id, currentThread->getTID());
}


void SwapThread::requestSwapRemoveEntry()
{
  debug(SWAPREQUEST, "requestSwapRemoveEntry() by [%ld]\n", currentThread->getTID());
  assert(false && "how about look at which things you uncomment? @SwapThread::requestsSolve()");
} 





// -----------------------------------------------------------------------------
//                           solving them requests
// -----------------------------------------------------------------------------

void SwapThread::Run()
{
  debug(SWAPTHREAD, "Run() called.\n");
  while(true)
  {
    // TODO sleep if scheduled without requests
    if(requestSolveSwap())
      Scheduler::instance()->yield();
    else 
      debug(SWAPTHREAD, "Run() found solved 1 round of requests.\n");
  }
}


bool SwapThread::requestSolveSwap()
{
  // these 3 methods return true if empfty
  bool empty1 = true; //requestSolveSwapRemoveEntry();
  bool empty2 = requestSolveSwapIn();
  bool empty3 = requestSolveSwapOut();

  // if everything is empfty -> yield
  bool yield = empty1 && empty2 && empty3;
  assert(yield && "a request to SwapThread was sent but solving requests not implemented.");
  return yield;
}


bool SwapThread::requestSolveSwapRemoveEntry()
{
  lock_requests_swap_remove_entry_.acquire();
  debug(SWAPTHREAD, "requestSolveSwapRemoveEntry() has size %ld\n", requests_swap_remove_entry_.size());
  for(size_t i = 0; i < requests_swap_remove_entry_.size(); i++)
  {
    debug(SWAPTHREAD, "TODO: delete entry? what did we even store? delete from where?\n");
  }
  assert(false && "requestSolveSwapRemoveEntry() was uncommented...");
  lock_requests_swap_remove_entry_.release();
  return true;
}
     

bool SwapThread::requestSolveSwapOut()
{
  // return true if vector empfty
  lock_requests_swap_out_.acquire();
  if(requests_swap_out_.size() == 0)
  {
    lock_requests_swap_out_.release();
    return true;
  }

  // read request
  debug(SWAPTHREAD, "requestSolveSwapOut() found some requests\n");
  SwapOut request = requests_swap_out_.front();
  requests_swap_out_.erase(requests_swap_out_.begin());
  lock_requests_swap_out_.release();

  // this allocPPN() from SwapThread will land in swapOut()
  debug(SWAPTHREAD, "requestSolveSwapOut(): *(request->found_ptr) = allocPPN()\n");
  *(request.found_ptr) = PageManager::instance()->allocPPN();

  // signal sleeping thread to wake
  lock_requests_swap_out_.acquire();
  bool empty = requests_swap_in_.size() == 0;
  debug(SWAPTHREAD, "requestSolveSwapOut(): swapped out and count ppn at %d. signaling cond thread\n", *(request.found_ptr));
  request.cond_swap_out->signal();
  lock_requests_swap_out_.release();
  return empty;
}


bool SwapThread::requestSolveSwapIn()
{
  // return true if vector empty.
  lock_requests_swap_in_.acquire();
  if(requests_swap_in_.size() == 0)
  {
    lock_requests_swap_in_.release();
    return true;
  }

  // get request
  debug(SWAPTHREAD, "requestSolveSwapIn() has size %ld\n", requests_swap_in_.size());
  SwapIn request = requests_swap_in_.front();
  requests_swap_in_.erase(requests_swap_in_.begin());
  lock_requests_swap_in_.release();

  // assert on failed swapIn()... could also return a value to PageFaultHandler
  assert(swapIn(request.swap_id) && "swapIn(swap_id) failed with that swap_id");
  
  // wake waiting thread and return emptyness
  lock_requests_swap_in_.acquire();
  bool empty = requests_swap_in_.size() == 0;
  request.cond_swap_in->signal();
  lock_requests_swap_in_.release();
  return empty;
}





// -------------------------------------------------------------------------
//                        actually swap something.
// -------------------------------------------------------------------------

uint32 SwapThread::swapOut()
{
  debug(SWAPTHREAD, "swapOut(): entered\n");
  // PageManager*        _pm   = PageManager::instance();
  SwapManager*        _sm   = SwapManager::instance();
  ProcessRegistry*    _pr   = ProcessRegistry::instance();
  InvertedPageTable*  _ipt  = InvertedPageTable::instance();

  _ipt->lockIPT();   // lock ipt before or after PRA?
  
  // find a valid ppn to swap
  uint32 ppn = PageReplacementAlgos::randomPRA(); //pm->getPPN(); // wo funktion?
  assert(ppn);
  uint32 swap_id = swap_cnt_++;
  IPTE* ipte = _ipt->getEntry(ppn);
  assert(ipte->progs_mappings.size() != 0 && "how did that page end up here without a proc?");
  debug(SWAPTHREAD, "swapOut(): got ppn %x from PRA which has %ld procs on it the first being %ld\n", ppn,ipte->progs_mappings.size(), ipte->progs_mappings[0].first->getPID());
  assert(ipte && "swapOut(): PRA ppn was not in IPT");
  assert(!ipte->my_flags.swapped && "ipte for that swap_id said swapped = 1");

  // lock archmems like in IPT::deduplicate() o,o
  bool prog_safe = false;
  ustl::map<size_t, UserProcess*> lock_process;

  while (!prog_safe)
  {
    lock_process.clear();
    for (auto process : ipte->progs_mappings)
    {
      debug(SWAPTHREAD, "emplacing: [%ld]\n", process.first->getPID());
      lock_process.emplace(process.first->getPID(), process.first);
    }
    prog_safe = _pr->lockMultArchmem(lock_process);
    if (!prog_safe)
    {
      debug(SWAPTHREAD, "swapOut(): could not lockMultArchmem(processes)...yielding\n");
      Scheduler::instance()->yield();
    }
  }

  // write to disk
  assert(_sm->writeToDisk(swap_id, ppn)); // wo funktion?

  // for every archmem that looks onto this page: set page bits
  for (auto process : ipte->progs_mappings)
    assert(process.first->getLoader()->arch_memory_.setPageToSwapOut(swap_id, ppn, process.second));

  _pr->unlockMultArchmem(lock_process);
  
  // remove ppn key entry from ipt & add under new key <swap page number>
  ipte->my_flags.swapped = true;
  IPTE tmp = *ipte;
  assert(_ipt->deleteEntry(ppn) && "swapOut():physical page number was not in ipt"); 
  assert(_ipt->addEntry(swap_id, tmp) && "swapOut():swap page number already in ipt"); // this will not work because we use a pointer, right?
  
  _ipt->unlockIPT();

  return ppn;
}


uint32 SwapThread::swapIn(size_t swap_id)
{
  debug(SWAPTHREAD, "swapIn(swap_id = %lx): entered\n", swap_id);
  PageManager*        _pm   = PageManager::instance();
  SwapManager*        _sm   = SwapManager::instance();
  ProcessRegistry*    _pr   = ProcessRegistry::instance();
  InvertedPageTable*  _ipt  = InvertedPageTable::instance();

  _ipt->lockIPT();

  // find page?
  IPTE* ipte = _ipt->getEntry(swap_id);
  if (!ipte)
  {
    debug(SWAPTHREAD, "looks like I have already dealt with that swap in and the page is back in already! returning...\n");
    return 1;
  }
  
  assert(ipte->my_flags.swapped && "ipte for that swap_id said swapped = 0");
  
  // lock archmems like in IPT::deduplicate() o,o
  bool prog_safe = false;
  ustl::map<size_t, UserProcess*> lock_process;

  while (!prog_safe)
  {
    lock_process.clear();
    for (auto process : ipte->progs_mappings)
    {
      debug(SWAPTHREAD, "emplacing: [%ld]\n", process.first->getPID());
      lock_process.emplace(process.first->getPID(), process.first);
    }
    prog_safe = _pr->lockMultArchmem(lock_process);
    if (!prog_safe)
    {
      debug(SWAPTHREAD, "swapIn(swap_id = %lx): could not lockMultArchmem(processes)...yielding\n", swap_id);
      Scheduler::instance()->yield();
    }
  }

  // read from swap page
  uint32 ppn = _pm->allocPPN();
  assert(_sm->readFromDisk(swap_id, ppn) && "could not read requested swap_id from SwapManager"); // readFromDisk return 0 on fail?

  // for every archmem that looks onto this page: set page bits
  for (auto process : ipte->progs_mappings)
    assert(process.first->getLoader()->arch_memory_.setPageToSwapIn(swap_id, ppn, process.second) && "setPageToSwapIn() failed.");
  
  _pr->unlockMultArchmem(lock_process);
  
  // remove swap_id key entry from ipt & add under new key <swap page number>
  ipte->my_flags.swapped = false;
  IPTE tmp = *ipte;
  assert(_ipt->deleteEntry(swap_id) && "physical page number was not in ipt"); 
  assert(_ipt->addEntry(ppn, tmp) && "ppn page number already in ipt"); // this will not work because we use a pointer, right?
  
  _ipt->unlockIPT();
  
  return ppn;
}


