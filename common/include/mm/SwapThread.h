#pragma once

#include "Thread.h"
#include "uvector.h"
#include "Scheduler.h"
#include "SwapManager.h"
#include "Mutex.h"
#include "PageManager.h"
#include "ProcessRegistry.h"

#define SWAPTHREAD_LOAD 8
// swap_id should start at 0x800 that way
#define SICHERHEITSABSTAND 32

// Pagefault needs disc -> ram.
struct SwapIn
{
  Condition* cond_request_swap_in_;
  // the id with which we'll find the page on the swap disc
  uint32 swap_id_;
};

// allocPPN() needs ram -> disk
struct SwapOut
{
  Condition* cond_request_swap_out_;
  // pointer to variable "found" in allocPPN
  uint32* found_ptr_;
};


/* NO RAM for allocPPN(): swapOut(ram -> disk).
 * PAGEFAULT because swapOut(): swapIn(disc -> ram).
 */
class SwapThread : public Thread
{
  public:
    // -------------------------------------------------------------------------
    //                             basic setup
    // -------------------------------------------------------------------------
    SwapThread();
    static SwapThread* instance();



    // -------------------------------------------------------------------------
    //                         UserThread requests
    // -------------------------------------------------------------------------

    /**
     * @brief Called when receiving a pagefault.
     * Send request. 
     * Sleep until signaled in swapIn().
     * 
     * @param swap_id The swap_id of the page that we stored.
     * At swapOut we set ppn to swap_id
     */
    void requestSwapInAndSleep(uint32 swap_id);

    /**
     * @brief called in allocPPN(). 
     * Send request, sleep.
     * Sleep until signaled in swapIn().
     * 
     * @param found_ptr 
     */
    void requestSwapOutAndSleep(uint32* found_ptr);

    /**
     * @brief request to delete a page on swap partition
     * 
     * @param swap_id the id to that page
     */
    void requestSwapRemoveEntry(uint32 swap_id);




  private:
    // -------------------------------------------------------------------------
    //                            SwapThread solve requests
    // -------------------------------------------------------------------------

    /**
     * @brief  Resolve requests all day every day.
     * @return Nothing. Is a void... But gets sleepy quickly...
     */
    virtual void Run();

    /**
     * @brief  Will solve 1 request of each type.
     * @return True if no requests.
     */
    bool requestSolveSwap();
    
    /**
     * @brief  Requests a page that has been swappednend away. 
     * @return True if nothing in queue.
     */
    bool requestSolveSwapIn();
    
    /**
     * @brief  Requests to swappen out.
     * @return True if nothing in queue.
     */
    bool requestSolveSwapOut();

    /**
     * @brief NOTHING
     * @return assert
     */
    bool requestSolveSwapRemoveEntry();

    // -------------------------------------------------------------------------
    //                        actually swap something.
    // -------------------------------------------------------------------------

    /**
     * @brief Move PRA-selected page from ram -> disk.
     * @return uint32 the ppn of now free ppn
     */
    uint32 swapOut();

    /**
     * @brief 
     * 
     * @param swap_id id under which we stored the page
     * @return uint32 the new ppn
     */
    uint32 swapIn(size_t swap_id);


    

    // -------------------------------------------------------------------------
    //                              Thread requests
    // -------------------------------------------------------------------------

    /**
     * @brief A counter that increases with every swapOut().
     * Used to find a block on swap partition.
     */
    uint32 swap_cnt_;

    /**
     * Requests that Threads left before going to sleep.
     * + lock.
     */
    ustl::vector<SwapIn> requests_swap_in_;
    Mutex lock_requests_swap_in_;
    
    /**
     * Requests that Threads left before going to sleep.
     * + lock.
     */
    ustl::vector<SwapOut> requests_swap_out_;
    Mutex lock_requests_swap_out_;
    
    /**
     * holds swap_ids that shall be removed
     * + lock
     */
    ustl::vector<uint32> requests_swap_remove_entry_;
    Mutex lock_requests_swap_remove_entry_;


    // -------------------------------------------------------------------------
    //                          SwapThread
    // -------------------------------------------------------------------------

    static SwapThread* instance_;

    Mutex lock_sleep_swap_thread_;
    Condition cond_sleep_swap_thread_;
};