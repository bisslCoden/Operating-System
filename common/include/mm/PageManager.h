#pragma once

#include "types.h"
#include "paging-definitions.h"
#include "SpinLock.h"
#include "Bitmap.h"
#include "umap.h"
#include "Mutex.h"
#include "UserProcess.h"
#include "InvertedPageTable.h"
#include "uqueue.h"

#define DYNAMIC_KMM (0) // Please note that this means that the KMM depends on the page manager
// and you will have a harder time implementing swapping. Pros only!

//#define WAS_LAST 0x82426784

class UserProcess;
class PageManager
{
  public:
    static PageManager *instance();

    /**
     * returns the number of 4k Pages avaible to sweb.
     * ((size of the first usable memory region or max 1Gb) / PAGESIZE)
     * @return number of available pages
     */
    uint32 getTotalNumPages() const;

    /**
     * returns the number of currently free pages
     * @return number of free pages
     */
    size_t getNumFreePages() const;

    /**
     * returns the number of the lowest free Page
     * and marks that Page as used.
     * returns always 4kb ppns!
     */
    uint32 allocPPN(uint32 page_size = PAGE_SIZE);

    /**
     * marks physical page <page_number> as free, if it was used in
     * user or kernel space.
     * @param page_number Physcial Page to mark as unused
     */
    void freePPN(uint32 page_number, uint32 page_size = PAGE_SIZE);

    Thread* heldBy()
    {
      return lock_.heldBy();
    }

    PageManager();

    void printBitmap()
    {
      page_usage_table_->bmprint();
    }
    
    void allocPagesAndAddQueue(size_t num_pages, ustl::queue<size_t>* ppns);
    void freeRestOfPages(ustl::queue<size_t>* ppns);


    /**
     * @brief Handles a cow Pagefault and refreshes the cow ist accordingly 
     * IMPORTANT: Locking Policy: 1st aquire Pagemanager::cnt_lock_ and then 
     * acquire the Archmem lock of the CURRENT PROCESS
     * 
     * @param address the Pagefault adress
     * @return true if Pagefault could be handled correctly 
     * @return false else
     */
    bool checkForCow(size_t address);

  private:
    /**
     * used internally to mark pages as reserved
     * @param ppn
     * @param num
     * @return
     */
    bool reservePages(uint32 ppn, uint32 num = 1);

    PageManager(PageManager const&);

    Bitmap* page_usage_table_;
    uint32 number_of_pages_;
    uint32 lowest_unreserved_page_;

    SpinLock lock_;

    static PageManager* instance_;

    size_t HEAP_PAGES;

};
