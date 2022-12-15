#include "SwapManager.h"
#include "PageManager.h"
#include "debug.h"

SwapManager* SwapManager::instance_ = 0;

SwapManager* SwapManager::instance()
{
  if (unlikely(!instance_))
    instance_ = new SwapManager();
  return instance_;
}

SwapManager::SwapManager()
{
  debug(SWAPMANAGER, "SwapManager constructor called!\n");
  assert(instance_ == 0);
  instance_ = this;

  device_ = BDManager::getInstance()->getDeviceByNumber(DEVICE_NUMBER);
  device_->setBlockSize(PAGE_SIZE);
  max_blocks_ = device_->getNumBlocks();
  lowest_block_ = 0;
  used_pages_ = new Bitmap(max_blocks_);
}


size_t SwapManager::allocSPN()
{
  size_t spn = lowest_block_;
  if(lowest_block_ >= max_blocks_)
    assert(false && "No more space on virtual Device\n");
  
  //used_pages_->setBit(lowest_block_);
  while((spn < max_blocks_) && used_pages_->getBit(spn))
  {
    ++spn;
  }
  debug(SWAPMANAGER, "found free swap page on page %lx!\n", spn);
  assert(spn < max_blocks_);
  used_pages_->setBit(spn);
  char buff[PAGE_SIZE] = {0};
  device_->writeData((spn * device_->getBlockSize()),device_->getBlockSize(), buff);
  return spn;
}

void SwapManager::freeSPN(size_t spn)
{
  assert(spn < max_blocks_);
  used_pages_->unsetBit(spn);
  return;
}




// bool SwapManager::checkPresence(size_t swap_pn)
// {
//   // lock it
//   if(swap_map_.find(swap_pn) != swap_map_.end())
//   {
//     debug(SWAPMANAGER, "Swapped page IN swap_map_!\n");
//     return true;
//   }
//   debug(SWAPMANAGER, "Swapped page NOT in swap_map_!\n");
//   return false;
// }

// void SwapManager::addEntry(size_t swap_pn, size_t vpn, ArchMemory* arch_memory)
// {
//   // locking?
//   ArchMemoryMapping m = arch_memory->resolveMapping(vpn);
//   assert(m.pt[m.pti].present && m.pt[m.pti].swap && "NOT swapped OR NOT present !!!!");

//   ustl::map<ArchMemory*, size_t> arch_vpn_buffer;
//   size_t pml = 1; // will need to provide the lvl somehow, or just remove it
//   assert(vpn > 0 && "Virtual Page Number is equal to 0!\n");
//   debug(SWAPMANAGER, "Adding the Swap entry to the swap_map_\n");
//   //handling for swapping
//   if(checkPresence(swap_pn))
//   {
//     if(swap_map_.at(swap_pn).arch_memory_vpn_.find(arch_memory) != swap_map_.at(swap_pn).arch_memory_vpn_.end())
//     {
//       if(swap_map_.at(swap_pn).arch_memory_vpn_.at(arch_memory) == vpn)
//       {
//         assert(false && "Page already found in swap_map_ what are you doing??\n");
//       }else
//       {
//         assert(false && "Cant come here!!\n");
//       }
//     }
//     debug(SWAPMANAGER, "Page in swap_map_ at %ld!!\n",swap_pn);
//     arch_vpn_buffer = swap_map_.at(swap_pn).arch_memory_vpn_;
//     arch_vpn_buffer.insert({arch_memory, vpn});
//   }else
//   {
//     debug(SWAPMANAGER, "Page not in swap_map_ adding  %ld to the map!!\n", swap_pn);
//     arch_vpn_buffer.insert({arch_memory, vpn});
//   }
//   Swap_Info info = {arch_vpn_buffer, pml};
//   swap_map_[swap_pn] = info;
// }

// void SwapManager::deleteEntry(size_t swap_pn, size_t vpn, ArchMemory* arch_memory)
// {
//   assert(vpn > 0 && "Virtual Page Number is equal to 0!\n");

//   if(swap_map_.at(swap_pn).arch_memory_vpn_.size() == 1)
//   {
//    swap_map_.erase(swap_pn);
//   }
//   if(swap_map_.at(swap_pn).arch_memory_vpn_.size() == 2)
//   {
//     // lock when using archmem!!!

//     size_t s_vpn = swap_map_.at(swap_pn).arch_memory_vpn_.begin()->second;
//     auto m = swap_map_.at(swap_pn).arch_memory_vpn_.begin()->first->resolveMapping(s_vpn);
//     m.pt[m.pti].writeable = 1;
//     m.pt[m.pti].cow = 0;

//     swap_map_.at(swap_pn).arch_memory_vpn_.erase(arch_memory);
//   }else
//   {
//     swap_map_.at(swap_pn).arch_memory_vpn_.erase(arch_memory);
//   }
// }

void SwapManager::writeToDisk(uint32 swap_ID, uint32 ppn)
{
  size_t spn = allocSPN();
  swapID_to_spn_map_.emplace(swap_ID, spn);
 
  device_->writeData((spn * device_->getBlockSize()),device_->getBlockSize(), (char*)ArchMemory::getIdentAddressOfPPN(ppn));
  return;
}

bool SwapManager::readFromDisk(uint32 swap_ID, uint32 ppn)
{
  if (swapID_to_spn_map_.find(swap_ID) == swapID_to_spn_map_.end())
    return false;
  if(ppn == 0)
    return false;

  char* page = (char*) ArchMemory::getIdentAddressOfPPN(ppn);
  device_->readData(swapID_to_spn_map_[swap_ID], PAGE_SIZE, page);
  freeSPN(swapID_to_spn_map_[swap_ID]);
  swapID_to_spn_map_.erase(swap_ID);
  return true;
}

bool SwapManager::deleteFromDisk(uint32 swap_ID)
{
  if (swapID_to_spn_map_.find(swap_ID) == swapID_to_spn_map_.end())
    return false;
  
  freeSPN(swapID_to_spn_map_[swap_ID]);
  swapID_to_spn_map_.erase(swap_ID);
  return true;
}

