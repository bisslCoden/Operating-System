#pragma once

#include "Thread.h"
#include "umap.h"
#include "UserThread.h"
#include "Syscall.h"
#include "uvector.h"
#include "Loader.h"

#define NO_EXEC_CALL 123454321

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
     * @param parent parent process that shall be forked
     *
     */
    UserProcess(UserProcess* parent);

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
     * @brief creates a thread that starts the binary of the program
     * 
     * @param path the path to the binary
     * @param argv the arguments 
     * @param argc the argument count
     */
    int execv(const char* path, char *const argv[], size_t argc);
    // this is non-args version of execv
    int execv(const char* path);

    void removeOldProcessInformation();

    /**
     * @brief does the Loader setup + fd check
     * 
     * @param fd the fd that's used in the Loader constructor (use local variable, not member fd_)
     * @return true if success,
     * @return false if rip
     */
    bool setupLoader(ssize_t fd);

    /**
     * @brief UNSAFELY removes userthread from threads_ 
     * 
     * @param thread the userthread
     * @return true if found in list
     * @return false if not found
     */
    bool removeFromThreadList(UserThread* thread);

    /**
     * @brief UNSAFELY searches TID in threads_ LOCK BEFORE AND RELEASE AFTER CALL
     * 
     * @param tid the tid
     * @return Thread* pointer to the thread (0 if not found)
     */
    Thread* findInThreadList(size_t tid);
    
    /**
     * @brief safely adds a thread's return value to returnvalues_
     * maps the value to its TID
     * 
     * @param tid the tid
     * @param value pointer to the value
     * @return true if success, 
     * @return false if already in list (assert)
     */
    bool addToRetvalList(size_t tid, void* value);

    /**
     * @brief returns threads_.size() but threadsafe
     * 
     * @return size_t the numer of threads
     */
    size_t getNrOfThreads();

   /**
     * @brief returns a random offset generated with rdtsc. This should only be used to set
     * UserThread::mystack_.page_offset_!!
     *  
     * @return the offset AS VPN
     */
    size_t getRandomPageOffset();

    /**
     * @brief Create a New Thread object (pthread_create)
     * 
     * @param start_routine which thread should execute
     * @return size_t thread ID
     */
    size_t createNewThread(size_t start_routine, size_t args, size_t wrapper, int32 joinstate);

    /**
     * @brief exits the whole userprogram if kill_last is true. 
     * otherwise only destroys all except currentThread
     * 
     * @param exit_code the exit code
     * @param kill_last bool that says whether currentThread should be killed.
     */
    void exit(size_t exit_code, bool kill_currentThread = true);

    void lockKill(){kill_lock_.acquire();}
    void unlockKill(){kill_lock_.release();}
    bool checkKill(){ return KILLED_;}


    void lockThreadMutex(){threads_lock_.acquire();}
    void unLockThreadMutex(){threads_lock_.release();}

    // getters
    size_t getPID()                 { return pid_; }
    Loader* getLoader()             { return loader_; }
    FileSystemInfo* getWorkingDir() { return working_dir_; }
    ustl::string getName()          { return name_; }
      /**
     * @brief a retval is REMOVED from the retvallist and given to the joining thread
     * 
     * @param tid the userthread id which should be found
     * @param value place where the returnvalue is saved into
     * 
     * @return false if the Thread was not in the list
     */
    bool getRetVal(size_t tid, void** value);
    bool checkInOffsetList(size_t NR);

  private:
    // the process ID
    size_t const pid_;

    // the process' fd. see "FileDescriptor.h"
    ssize_t fd_;

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

    // a list mapping TIDs and their appropriate UserThread*
    ustl::map<size_t, UserThread*> threads_;
    Mutex threads_lock_;

    // a list mapping a return value to a TID
    ustl::map<size_t, void*> returnvalues_;
    Mutex returnvalue_lock_;

    // the offsets of the thread's stack
    ustl::vector<size_t> offsets_;
    Mutex offsetlist_lock_;

    Mutex kill_lock_;
    bool KILLED_ = false;
};

