#include "SwapThread.h"
#include "debug.h"

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
  lock_requests_swap_remove_entry_("SwapThread::lock_requests_swap_remove_entry_"),
  lock_sleep_swap_thread_("SwapThread::lock_sleep_swap_thread_"),
  cond_sleep_swap_thread_(&lock_sleep_swap_thread_, "SwapThread::cond_sleep_swap_thread_")
{
  swap_cnt_ = PageManager::instance()->getTotalNumPages() + SICHERHEITSABSTAND;
  debug(SWAPTHREAD, "SwapThread(). initialized swap_cnt_ to %x\n", swap_cnt_);
}


// -----------------------------------------------------------------------------
//                           getting them requests
// -----------------------------------------------------------------------------

void SwapThread::requestSwapOutAndSleep(uint32* found_ptr)
{
  debug(SWAPREQUEST, "requestSwapOutAndSleep(found_ptr = %lx) by [%ld] name: %s. Will push my request, wake SwapThread and sleep...\n", 
    (size_t)found_ptr, currentThread->getTID(), currentThread->getName());
  assert(found_ptr && ((*found_ptr) == 0) && "wat?");

  // SwapThread better not 
  if(currentThread->getType() == KERNEL_THREAD)
  {
    assert(currentThread == this && "KernelThread calling requestSwapOutAndSleep was not SwapThread.");
    debug(SWAPREQUEST, "SwapThread's allocPPN() sent a requestSwapOut() Better freePPN(swapOut())\n");
    for(int i = 0; i < SWAPTHREAD_LOAD; i++)
      PageManager::instance()->freePPN(swapOut());
    *found_ptr = swapOut();
    return;
  }

  // create request
  Condition request_cond(&lock_requests_swap_out_ , "SwapThread::SwapOut::cond_swap_out");
  SwapOut request{};
  request.found_ptr_ = found_ptr;
  request.cond_request_swap_out_ = &request_cond;

  // push request, signal swapthread and sleep
  // (should we change the locking to: 1) lock request, push request, unlock request. 2) lock sleep, signal sleep, unlock sleep 3) lock requests, sleep on requests)?
  debug(SWAPREQUEST, "TIP [%ld] with (found_ptr = %lx) by will signal SwapThread and go to sleep\n", 
    currentThread->getTID(), (size_t)found_ptr); 
  lock_requests_swap_out_.acquire();                  // 1 lock: request acquire
  requests_swap_out_.push_back(request);              // push request
  lock_sleep_swap_thread_.acquire();                  // 2 lock: swapthread sleep sleep
  cond_sleep_swap_thread_.signal();                   // signal
  lock_sleep_swap_thread_.release();                  // 2 lock: request release
  request.cond_request_swap_out_->waitAndRelease();   // 1 lock: wait for cond on lock 1

  // awake again
  debug(SWAPREQUEST, "requestSwapOut() by TID [%ld] will continue with found_ptr = %lx\n", 
    currentThread->getTID(), (size_t)found_ptr);
  assert(request.found_ptr_ && "requesSwapOut() woke up and found_ptr became 0!?!");
}


void SwapThread::requestSwapInAndSleep(uint32 swap_id)
{
  debug(SWAPREQUEST, "requestSwapInAndSleep(swap_id = %x) by [%ld] name: %s. Will push my request, wake SwapThread and sleep...\n", 
    swap_id, currentThread->getTID(), currentThread->getName());
  assert(currentThread->getTID() && "requestSwapInAndSleep() was called by a kernelthread?");

  // create request 
  Condition request_cond(&lock_requests_swap_in_, "SwapThread::SwapIn::cond_swap_in");
  SwapIn request{};
  request.swap_id_ = swap_id;
  request.cond_request_swap_in_ = &request_cond;

  // push request, signal swapthread and sleep 
  // (should we change the locking to: 1) lock request, push request, unlock request. 2) lock sleep, signal sleep, unlock sleep 3) lock requests, sleep on requests)?
  debug(SWAPREQUEST, "requestSwapInAndSleep(swap_id = %x) by TID [%ld] will signal SwapThread and go to sleep\n", 
    swap_id, currentThread->getTID()); 
  lock_requests_swap_in_.acquire();                   // 1 lock: request acquire
  requests_swap_in_.push_back(request);               // push request
  lock_sleep_swap_thread_.acquire();                  // 2 lock: swapthread sleep sleep
  cond_sleep_swap_thread_.signal();                   // signal
  lock_sleep_swap_thread_.release();                  // 2 lock: request release
  request.cond_request_swap_in_->waitAndRelease();    // 1 lock: wait for cond on lock 1

  // awake again
  debug(SWAPREQUEST, "requestSwapInAndSleep(ppn = %x) by [%ld] woke up again\n", swap_id, currentThread->getTID());
}


void SwapThread::requestSwapRemoveEntry(uint32 swap_id)
{
  debug(SWAPREQUEST, "requestSwapRemoveEntry(swap_id = %x) by [%ld]. immediately pushing back.\n", swap_id, currentThread->getTID());
  lock_requests_swap_remove_entry_.acquire();
  requests_swap_remove_entry_.push_back(swap_id);
  lock_requests_swap_remove_entry_.release();
} 





// -----------------------------------------------------------------------------
//                           solving them requests
// -----------------------------------------------------------------------------

void SwapThread::Run()
{
  debug(SWAPTHREAD, "Run(): called.\n");
  while(true)
  {
    if(requestSolveSwap())
    {
      debug(SWAPTHREAD, "Run(): No requests... bored... sleeping now\n");
      lock_sleep_swap_thread_.acquire();
      cond_sleep_swap_thread_.waitAndRelease();
      debug(SWAPTHREAD, "Run(): Woke up again. Feeling quite good, well rested. \n");
    }
  }
}


bool SwapThread::requestSolveSwap()
{
  // these 3 methods return true if empfty
  debug(SWAPTHREAD, "requestSolveSwap() entered\n");
  bool empty1 = requestSolveSwapRemoveEntry();
  bool empty2 = requestSolveSwapIn();
  bool empty3 = requestSolveSwapOut();

  // if everything is empfty -> yield
  bool yield = empty1 && empty2 && empty3;
  assert(yield && "a request to SwapThread was sent but solving requests not implemented.");
  return yield;
}


bool SwapThread::requestSolveSwapRemoveEntry()
{
  debug(SWAPTHREAD, "requestSolveSwapRemoveEntry() entered\n");

  // return true if vector empfty
  lock_requests_swap_remove_entry_.acquire();
  if(requests_swap_remove_entry_.size() == 0)
  {
    lock_requests_swap_remove_entry_.release();
    return true;
  }

  // read every request and remove
  // SwapManager*       _sm  = SwapManager::instance(); // ----> uncomment when removeFromDisk() available
  InvertedPageTable* _ipt = InvertedPageTable::instance();
  debug(SWAPTHREAD, "requestSolveSwapRemoveEntry() has size %ld\n", requests_swap_remove_entry_.size());
  for(size_t swap_id : requests_swap_remove_entry_)
  {
    _ipt->deleteEntry(swap_id);
    // _sm->removeFromDisk(swap_id); // ----> uncomment when available 
  }
  lock_requests_swap_remove_entry_.release();
  return true;
}
     

bool SwapThread::requestSolveSwapOut()
{
  debug(SWAPTHREAD, "requestSolveSwapOut() entered\n");

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

  // this allocPPN() from SwapThread will land in  swapOut()
  debug(SWAPTHREAD, "requestSolveSwapOut(): *(request->found_ptr) = allocPPN()\n");
  assert(request.found_ptr_ && "wtf?");
  *(request.found_ptr_) = PageManager::instance()->allocPPN();

  // signal sleeping thread to wake
  lock_requests_swap_out_.acquire();
  bool empty = requests_swap_in_.size() == 0;
  assert(request.found_ptr_ && "wtf?");
  debug(SWAPTHREAD, "requestSolveSwapOut(): swapped out and count ppn at %d. signaling cond thread\n", *(request.found_ptr_));
  request.cond_request_swap_out_->signal();
  lock_requests_swap_out_.release();
  return empty;
}


bool SwapThread::requestSolveSwapIn()
{
  debug(SWAPTHREAD, "requestSolveSwapIn() entered\n");

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
  assert(swapIn(request.swap_id_) && "swapIn(swap_id) failed with that swap_id");
  
  // wake waiting thread and return emptyness
  lock_requests_swap_in_.acquire();
  bool empty = requests_swap_in_.size() == 1;
  request.cond_request_swap_in_->signal();
  lock_requests_swap_in_.release();
  return empty;
}





// -------------------------------------------------------------------------
//                        actually swap something.
// -------------------------------------------------------------------------

uint32 SwapThread::swapOut()
{
  debug(SWAPTHREAD, "swapOut(): entered\n");
  // PageManager*        _pm   = PageManager::instance(); // ----> uncomment if getPPN from PRA available
  // SwapManager*        _sm   = SwapManager::instance(); // ----> uncomment if writeToDisk available 
  ProcessRegistry*    _pr   = ProcessRegistry::instance();
  InvertedPageTable*  _ipt  = InvertedPageTable::instance();

  _ipt->lockIPT();   // lock ipt before or after PRA?
  
  // find a valid ppn to swap
  uint32 ppn = 0; //pm->getPPN(); // wo funktion?
  uint32 swap_id = swap_cnt_++;
  IPTE* ipte = _ipt->getEntry(ppn);
  debug(SWAPTHREAD, "swapOut(): got ppn %x from PRA\n", ppn);
  assert(ipte && "swapOut(): PRA ppn was not in IPT");
  assert(!ipte->my_flags.swapped && "ipte for that swap_id said swapped = 1");

  // lock archmems like in IPT::deduplicate() o,o
  bool prog_safe = false;
  ustl::map<size_t, UserProcess*> lock_process;
  for (auto process : ipte->progs_mappings)
    lock_process.push_back(ustl::make_pair(process.first->getPID(), process.first));
  while (!prog_safe)
  {
    lock_process.clear();
    prog_safe = _pr->lockMultArchmem(lock_process);
    if (!prog_safe)
    {
      debug(SWAPTHREAD, "swapOut(): could not lockMultArchmem(processes)...yielding\n");
      Scheduler::instance()->yield();
    }
  }

  // write to disk
  // assert(_sm->writeToDisk(swap_id, ppn)); // wo funktion?

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
  // SwapManager*        _sm   = SwapManager::instance();
  ProcessRegistry*    _pr   = ProcessRegistry::instance();
  InvertedPageTable*  _ipt  = InvertedPageTable::instance();

  _ipt->lockIPT();

  // find page?
  IPTE* ipte = _ipt->getEntry(swap_id);
  assert(ipte && "requested swap_id was not in IPT");
  assert(ipte->my_flags.swapped && "ipte for that swap_id said swapped = 0");
  
  // lock archmems like in IPT::deduplicate() o,o
  bool prog_safe = false;
  ustl::map<size_t, UserProcess*> lock_process;
  for (auto process : ipte->progs_mappings)
    lock_process.push_back(ustl::make_pair(process.first->getPID(), process.first));
  while (!prog_safe)
  {
    lock_process.clear();
    prog_safe = _pr->lockMultArchmem(lock_process);
    if (!prog_safe)
    {
      debug(SWAPTHREAD, "swapIn(swap_id = %lx): could not lockMultArchmem(processes)...yielding\n", swap_id);
      Scheduler::instance()->yield();
    }
  }

  // read from swap page
  uint32 ppn = _pm->allocPPN();
  // assert(_sm->readFromDisk(swap_id, ppn) && "could not read requested swap_id from SwapManager"); // readFromDisk return 0 on fail?
  // ^^^^^^^^ uncomment when available

  // for every archmem that looks onto this page: set page bits
  for (auto process : ipte->progs_mappings)
    assert(process.first->getLoader()->arch_memory_.setPageToSwapIn(swap_id, ppn, process.second) && "setPageToSwapIn() failed.");
  
  _pr->unlockMultArchmem(lock_process);
  
  // remove swap_id key entry from ipt & add under new key <swap page number>
  ipte->my_flags.swapped = false;
  IPTE tmp = *ipte;
  assert(_ipt->deleteEntry(swap_id) && "physical page number was not in ipt"); 
  assert(_ipt->addEntry(ppn, tmp) && "swap page number already in ipt"); // this will not work because we use a pointer, right?
  
  _ipt->unlockIPT();
  
  return ppn;
}


