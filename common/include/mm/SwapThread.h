#pragma once

#include "Thread.h"
#include "uvector.h"
#include "Scheduler.h"
#include "SwapManager.h"
#include "Mutex.h"
#include "PageManager.h"
#include "ProcessRegistry.h"

#define SWAPTHREAD_LOAD 8
#define SICHERHEITSABSTAND 20

// Pagefault needs disc -> ram.
struct SwapIn
{
  Condition* cond_swap_in;
  uint32 swap_id;
};

// allocPPN() needs ram -> disk
struct SwapOut
{
  Condition* cond_swap_out;
  // pointer to variable found in 
  uint32* found_ptr;
};

// we probably won't need a struct here
struct SwapRemove
{};

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
    //                               requests
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
     * @brief Will asssert if called. 
     * Just delete? What did we even store? Delete from where?
     */
    void requestSwapRemoveEntry();




  private:
    // -------------------------------------------------------------------------
    //                            solve requests
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
    //                          el member variablos
    // -------------------------------------------------------------------------

    static SwapThread* instance_;

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
     * NOTHING
     * + lock
     */
    ustl::vector<SwapRemove> requests_swap_remove_entry_;
    Mutex lock_requests_swap_remove_entry_;
};