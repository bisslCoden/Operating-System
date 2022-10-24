#pragma once

#include "Thread.h"
#include "umap.h"
#include "UserThread.h"
#include "Syscall.h"
#include "uvector.h"


class UserThread;
class Syscall;
class UserProcess
{
  public:
    /**
     * Constructor
     * @param minixfs_filename filename of the file in minixfs to execute
     * @param fs_info filesysteminfo-object to be used
     * @param terminal_number the terminal to run in (default 0)
     *
     */
    UserProcess(ustl::string minixfs_filename, FileSystemInfo *fs_info, uint32 terminal_number = 0);
    /**
     * Constructor
     * @param parent parent process to fork
     * @param pid the id of the parent
     *
     */
    UserProcess(UserProcess* parent, size_t pid);

    ~UserProcess();



    /**
     * @brief safely adds a userthread to threads
     * 
     * @param thread the userthread
     * @return true if successful
     * @return false if already found in list
     */
    bool addToThreadList(UserThread* thread);

    /**
     * @brief safely removes userthread from threads_
     * 
     * @param thread the userthread
     * @return true if found in list
     * @return false if not found
     */
    bool removeFromThreadList(UserThread* thread);

   /**
     * @brief finds a thread in the Userprocess' Threadlist and returns 0x00 if
     * it couldnt be found. Lock before!!!
     * 
     * @param tid the userthread id which should be found
     * 
     * @return the Thread searched for
     */
    Thread* findInThreadList(size_t tid);
    
      /**
     * @brief add a returnvalue for a thread to the lise and acquires the lock!
     * 
     * @param tid the userthread id which should be found
     * @param value the returnvalue to be added
     * 
     * @return false if the Thread already has a retval in the list
     */
    bool addToRetvalList(size_t tid, void* value);

    size_t getPID(){ return pid_; }
    Loader* getLoader() { return loader_; }
    FileSystemInfo* getWorkingDir() { return working_dir_; }
    ustl::string getName() { return name_; }
    /**
     * @brief returns threads_.size() but threadsafe
     * 
     * @return size_t the numer of threads
     */
    size_t getNrOfThreads();

   /**
     * @brief returns a random stack offset generated with rdtsc. This can then be set in the
     * Userthread() to get a stack. 
     * 
     * @return the offset
     */
    size_t getRandomPageOffset();

    void lockThreadMutex(){threads_lock_.acquire();}
    void unLockThreadMutex(){threads_lock_.release();}


    /**
     * @brief Create a New Thread object (pthread_create)
     * 
     * @param start_routine which thread should execute
     * @return size_t thread ID
     */
    size_t createNewThread(size_t start_routine, size_t args, size_t wrapper);

    /**
     * @brief pushes all threads of process onto list and destroys (currentThread last)
     * 
     * @param exit_code 
     * @return size_t 
     */
    void exit(size_t exit_code);

    void killThread(UserThread* thread);

      /**
     * @brief a retval is REMOVED from the retvallist and given to the joining thread
     * 
     * @param tid the userthread id which should be found
     * @param value place where the returnvalue is saved into
     * 
     * @return false if the Thread was not in the list
     */
    bool getRetVal(size_t tid, void** value);
    bool checkInList(size_t NR);

  private:
    // the process ID
    size_t const pid_;


    // the process' fd. see "FileDescriptor.h"
    ssize_t const fd_;

    // information about the program. path...
    FileSystemInfo* const fs_info_;

    // loader loads the program
    Loader* loader_;

    // the directory
    FileSystemInfo* working_dir_;

    // tells us which terminal is used. i think not relevant for now
    Terminal* my_terminal_;

    // name of the process.
    ustl::string name_;

  
    // a list containing TIDs and their appropriate UserThread*
    ustl::map<size_t, UserThread*> threads_;
    Mutex threads_lock_;
    ustl::map<size_t, void*> returnvalues_;
    Mutex returnvalue_lock_;

    Mutex offsetlist_lock_;
    ustl::vector<size_t> offsets_;

    // map with tid + return value for join
};

