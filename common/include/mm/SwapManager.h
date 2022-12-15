#pragma once

#include "ArchMemory.h"
#include "BDVirtualDevice.h"
#include "BDManager.h"
#include "Mutex.h"
#include "umap.h"
#include "Bitmap.h"
#include "assert.h"

#define DEVICE_NUMBER 3

struct Swap_Info
{
  ustl::map<ArchMemory*, size_t> arch_memory_vpn_;
  size_t pml_;
};

class SwapManager
{
public:
  SwapManager();

  virtual ~SwapManager() {};

  static SwapManager* instance();








  /**
   * hHALLO HIER IST DER SWAPTHREAD,
   * ICH HÄTTE GERNE 3 FUNKTIONEN ZUM MITNEHMEN:
   * 1) writeToDisk(size_t swap_ID, uint32 ppn)
   * 2) readFromDisk(size_t swap_ID, uint32 ppn)
   * 3) removeFromDisk(size_t swap_ID)
   * danke, ciao. bussi und au revoir!
   */



  void writePageContent(size_t swap_ID, size_t ppn);

  size_t writeToDisk(size_t ppn);

  void deleteFromDisk(size_t swap_pn);

  /**
   * checks if the page number is in the swap_map_
   * @param swap_pn
   * @return true if pn in map, false if not
   */
  bool checkPresence(size_t swap_pn);

  void getPageContent(size_t swap_ID, char* buffer);

  /**
   * adds a page to the swap_map_ with all its information, cow handling included
   * @param vpn
   * @param arch_memory
   */
  void addEntry(size_t swap_pn, size_t vpn, ArchMemory* arch_memory);

  /**
   * removes a page from the swap_map_, cow handling included
   * @param swap_pn
   * @param vpn
   * @param arch_memory
   */
  void deleteEntry(size_t swap_pn, size_t vpn, ArchMemory* arch_memory);


private:
  static SwapManager* instance_;

  /**
   * swap_map_ is a multimap, used to store information for swapping
   */
  ustl::map<size_t, Swap_Info> swap_map_;
  
  BDVirtualDevice* device_;
  size_t max_blocks_;
  size_t lowest_block_;
  Bitmap* used_pages_;
};
