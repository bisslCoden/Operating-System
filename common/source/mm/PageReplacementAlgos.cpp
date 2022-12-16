#include "PageReplacementAlgos.h"
#include "InvertedPageTable.h"
#include "Scheduler.h"

uint32 PageReplacementAlgos::randomPRA()
{
  InvertedPageTable* IPT = InvertedPageTable::instance();
  bool again = true;
  size_t random{0}, count{0};
  uint32 page = 0;
  while (again)
  {
    random = (Scheduler::instance()->getRDTSC() % IPT->IPT_.size());
    count = 0;
    debug(SWAPTHREAD, "got random value %ld\n", random);
    auto iter = IPT->IPT_.begin();
    //find entry in IPT
    while (count < random)
    {
      count++;
      iter++;
    }
    if ((iter->second.page_map_level != 0) || (iter->second.my_flags.swapped))
    {
      debug(SWAPTHREAD, "could not use %lx because its either swapped or pml not 0\n", iter->first);
    }
    else
    {
      debug(SWAPTHREAD, "COULD use %lx because its fine!\n", iter->first);
      again = false;
      page = iter->first;
    }
  }
  return page;
}

uint32 PageReplacementAlgos::pseudoRandomPRA()
{
  return 0;
}