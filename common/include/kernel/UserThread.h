#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"
#include "uatomic.h"
#include "uqueue.h"

#define currentUserThread ((UserThread*) currentThread)

#define PTHREAD_CANCELED ((void *) -1)
#define STACK_SIZE_IN_PAGES 16ULL
#define USERMUTEX_INVALID ((size_t*)0x35436343)

class UserProcess;

enum cancelstate {
    PTHREAD_CANCEL_ENABLE,  // thread can be cancelled at cancellation point (before & after switch(syscall_number))
    PTHREAD_CANCEL_DISABLE  // thread cannot be cancelled until set to ENABLE
};

enum canceltype {
    PTHREAD_CANCEL_DEFERRED,    // thread can be cancelled only at cancellation points
    PTHREAD_CANCEL_ASYNCHRONOUS // can be cancelled any time.
};

enum joinbale {
    PTHREAD_CREATE_JOINABLE,  // return value for pthread_join available
    PTHREAD_CREATE_DETACHED   // return value for pthread_join unavailable
};

typedef struct Threadflags
{
  int  cancelable = PTHREAD_CANCEL_ENABLE;
  int  deferred   = PTHREAD_CANCEL_DEFERRED;
  int  joinable   = PTHREAD_CREATE_JOINABLE;
  bool cancelreq                    = false;
  ustl::atomic<bool> kcancelreq     = false;
  ustl::atomic<bool> knotcancelable = false;
  ustl::atomic<bool> kasynchronous  = false;
}Threadflags;

class Semaphore;
typedef struct StackInfo
{
  size_t userstack_start_     = 0;
  size_t userstack_end_       = 0;
  size_t page_offset_         = 0;
  size_t guardpage_front_nr_  = 0;
  size_t guardpage_back_nr_   = 0;
  ustl::atomic<size_t*> UserMutex;
} StackInfo;


class UserThread : public Thread
{
  public:
    bool schedulable() override;
    /**
     * Constructor for first_thread
     * @param minixfs_filename filename of the file in minixfs to execute
     * @param fs_info filesysteminfo-object to be used
     * @param terminal_number the terminal to run in (default 0)
     *
     */
    UserThread(UserProcess* process_, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number, size_t* returnto, ustl::queue<size_t>* ppns);
    
    /**
     * @brief Construct a new User Thread object for pthread_create()
     * 
     * @param wrapper the wrapper for implicit pthread_exit() call
     * @param page_offset offset for stack location
     * @param terminal_number the terminal to run in (default 0)
     */
    UserThread(size_t wrapper, size_t* returnto, ustl::queue<size_t>* ppns, uint32_t terminal_number = 0);

    /**
     * @brief Construct a new User Thread object for fork()
     * 
     * @param child the UserProcess of this new thread
     * @param parent_thread the thread of the parent_thread that called fork()
     */
    UserThread(UserProcess* child, UserThread* parent_thread, size_t* returnto);

    /**
     * @brief Destroy the User Thread object and check if(isLast()) { destroy parent_; } 
     * 
     */
    ~UserThread();

    /**
     * only asserts if called. 
     * "object of abstract class type "UserThread" is not allowed:C/C++(322)
     * UserProcess.cpp(56, 34): function "UserThread::Run" is a pure virtual function"
     */
    void Run() override { assert(false && "UserThread::Run() was called...\n"); }

    /**
     * @brief sets up rsp, allocates ppn, finds vpn,
     *
     * @return true stack set successfully
     * @return false stack not setup.
     */
    bool setupStack(ustl::queue<size_t>* ppns);
    bool reuseStack(StackInfo* old_stackinfo, ustl::queue<size_t>* ppns);


    /**
     * @brief UserThread-part of constructor. sets few members, creates stack in new archemory and 
     * copies arguments from userspace via ident mapping to location pointed to by rsi and rdi
     * 
     * @param argv note: this is the kernel_argv that must be free'd here!
     * @param argc the argument count.
     * @return error return values
     */
    int execv(char* const argv[], size_t argc, ustl::queue<size_t>* ppns);

    void signalExec()                 { exec_wait_.signal();}
    void waitExec()                   { exec_wait_.wait();}

    //lock retval before!
    bool detectCircularJoin(UserThread* to_be_joined);
    size_t getLastStart() {return last_start_; }

    void setLastStart(size_t time) {last_start_ = time;}

    size_t getTimeToWake() {return time_to_wake_; }

    void setTimeToWake(size_t time) {time_to_wake_ = time;}

    void setUserMutex(size_t* address) { mystack_.UserMutex = address; }

    /**
     * @brief join functions: locks and setters for the join mechanics. setJoiner needs to be locked!
     * 
     */
    void setJoinState(int state)      {myflags_.joinable = state;}
    int getJoinState()                {return myflags_.joinable;}

    void getNewStackPage(size_t adress, ustl::queue<size_t>* ppns);
    
    /**
     * @brief kills thread immediately if needed.
     * otherwise unmapPage() before
     * 
     * @param actually_die 
     */
    void freeMyPagesAndDie(bool actually_die);

    // acqurie retvallock before!   join_waiter_ = thread;
    void setJoiner(UserThread* thread){join_waiter_ = thread;}
    // join_cond_.wait();
    void waitJoin()                   {join_cond_.wait();}
    // join_cond_.signal();
    void signalJoin()                 {join_cond_.signal();}
    // flag_mutex_.isHeldBy(caller);
    bool checkFlagLock(Thread* caller){ return flag_mutex_.isHeldBy(caller);}
    // mystack_.page_offset_;
    size_t getPageOffset()            { return mystack_.page_offset_;}
    // return &mystack_;
    StackInfo* getStackInfo()          { return &mystack_; }



    void lockFlagMutex()              { flag_mutex_.acquire();}
    void unlockFlagMutex()            { flag_mutex_.release();}

    void setCancelState(int state);
    void setCancelType(int type);
    void sendCancelRequest();
    void setLast()                    { last_ = true;}    
    // instruction pointer (rip) set to pthread_exit()
    void reDirectToDeath();

    // getters
    Threadflags*  getflags()          { return &myflags_;}     //lock before!
    UserThread*   getJoiner()         { return join_waiter_;}  //lock before!
    void*         getUserstackStart() { return (void*)mystack_.userstack_start_; }
    UserProcess*  getProcess()        { return process_; }
    bool          isLast()            { return last_; }        // tells if thread is the last thread of its process (important: on thread destuction, the process is destroyed if set!)
  private:
    friend class Scheduler;
    // the process that contains this thread
    UserProcess* process_;

    UserThread* join_waiter_ = 0;
    Mutex flag_mutex_;
    Condition join_cond_;

    Threadflags myflags_;

    StackInfo mystack_;
    bool last_ = false; 

    ustl::vector<size_t> my_pages_;
    Mutex my_pages_lock_;
    Condition exec_wait_;
    ustl::atomic<bool> DYING_;
    
    // only true if removeFromThreadList() detects last thread to delete process

    //clock
    size_t last_start_;
    bool was_scheduled_ = 0;
    //sleep
    size_t time_to_wake_ = 0;
};

