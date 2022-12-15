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
  lock_requests_swap_remove_entry_("SwapThread::lock_requests_swap_remove_entry_")
{
  swap_cnt_ = PageManager::instance()->getTotalNumPages() + 1;
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
  bool empty = requests_swap_in_.size() == 1;
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
  bool empty = requests_swap_in_.size() == 1;
  request.cond_swap_in->signal();
  lock_requests_swap_in_.release();
  return empty;
}





// -------------------------------------------------------------------------
//                        actually swap something.
// -------------------------------------------------------------------------

uint32 SwapThread::swapOut()
{
  InvertedPageTable* ipt = InvertedPageTable::instance();
  // PageManager* pm = PageManager::instance();
  SwapManager* sm = SwapManager::instance();
  
  ipt->lockIPT();
  
  // we need to find a valid ppn from IPT to swap
  uint32 ppn = 0; //pm->getPPN();; 
  IPTE* entry = ipt->getEntry(ppn);




  // lock archmems like in deduplicate.
  
  // for every archmem that looks onto this page: set page bits

  // create swap page number, write to swap disk
  uint32 swap_id = swap_cnt_++;
  sm->writeToDisk(swap_id);

  // unlock archmems




  // remove ppn key entry from ipt & add under new key <swap page number>
  entry->my_flags.swapped = true;
  assert(ipt->deleteEntry(ppn) && "physical page number was not in ipt"); 
  assert(ipt->addEntry(swap_id, *entry) && "swap page number already in ipt"); // this will not work because we use a pointer, right?
  
  ipt->unlockIPT();


  return ppn;
}


uint32 SwapThread::swapIn(size_t swap_id)
{
  debug(SWAPTHREAD, "swapIn() requested swap_id %lx\n", swap_id);
  // SwapManager* sm = SwapManager::instance();
  // PageManager* pm = PageManager::instance();

  // find page?
  // assert(sm->checkPresence(swap_id)); // do I have to enter swap_id or swap page number? paramerter's called swap_pn
  
  // SwapManager writes onto allocated page
  // uint32 ppn = pm->allocPPN();
  // assert((sm->getPageContent(swap_id, ppn) == 0) && "reading requested swap_id from SwapManager"); // getPageContent should return something...

  // change swap bits



  // add to ipt -> requestSwapOut()? currentThread = SwapThread: swapOut()
  
  return 0;
}


