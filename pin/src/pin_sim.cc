// Jonathan Eastep, Harshad Kasture, Jason Miller, Chris Celio, Charles Gruenwald,
// Nathan Beckmann, David Wentzlaff, James Psota
// 10.12.08
//
// Carbon Computer Simulator
//
// This simulator models future multi-core computers with thousands of cores.
// It runs on today's x86 multicores and will scale as more and more cores
// and better inter-core communications mechanisms become available.
// The simulator provides a platform for research in processor architecture,
// compilers, network interconnect topologies, and some OS.
//
// The simulator runs on top of Intel's Pin dynamic binary instrumention engine.
// Application code in the absence of instrumentation runs more or less
// natively and is thus high performance. When instrumentation is used, models
// can be hot-swapped or dynamically enabled and disabled as desired so that
// performance tracks the level of simulation detail needed.

#include <iostream>
#include <assert.h>
#include <set>
#include <sys/syscall.h>
#include <unistd.h>

#include "pin.H"
#include "knobs.h"
#include "log.h"
#include "run_models.h"
#include "analysis.h"
#include "routine_replace.h"

// FIXME: This list could probably be trimmed down a lot.
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "syscall_model.h"
#include "user_space_wrappers.h"
#include "thread_manager.h"
#include "config_file.hpp"

config::ConfigFile *cfg;

INT32 usage()
{
   cerr << "This tool implements a multicore simulator." << endl;
   cerr << KNOB_BASE::StringKnobSummary() << endl;

   return -1;
}

void routineCallback(RTN rtn, void *v)
{
   string rtn_name = RTN_Name(rtn);
   bool did_func_replace = replaceUserAPIFunction(rtn, rtn_name);

   if (!did_func_replace)
      replaceInstruction(rtn, rtn_name);
}

// syscall model wrappers

void SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
   Core *core = Sim()->getCoreManager()->getCurrentCore();

   if (core)
   {
      UInt8 syscall_number = (UInt8) PIN_GetSyscallNumber(ctxt, std);
      SyscallMdl::syscall_args_t args;
      args.arg0 = PIN_GetSyscallArgument(ctxt, std, 0);
      args.arg1 = PIN_GetSyscallArgument(ctxt, std, 1);
      args.arg2 = PIN_GetSyscallArgument(ctxt, std, 2);
      args.arg3 = PIN_GetSyscallArgument(ctxt, std, 3);
      args.arg4 = PIN_GetSyscallArgument(ctxt, std, 4);
      args.arg5 = PIN_GetSyscallArgument(ctxt, std, 5);
      UInt8 new_syscall = core->getSyscallMdl()->runEnter(syscall_number, args);
      PIN_SetSyscallNumber(ctxt, std, new_syscall);
   }
}

void SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
   Core *core = Sim()->getCoreManager()->getCurrentCore();

   if (core)
   {
      carbon_reg_t old_return = 
#ifdef TARGET_IA32E
      PIN_GetContextReg(ctxt, REG_RAX);
#else
      PIN_GetContextReg(ctxt, REG_EAX);
#endif

      carbon_reg_t syscall_return = core->getSyscallMdl()->runExit(old_return);

#ifdef TARGET_IA32E
      PIN_SetContextReg(ctxt, REG_RAX, syscall_return);
#else
      PIN_SetContextReg(ctxt, REG_EAX, syscall_return);
#endif
   }
}

void ApplicationStart()
{
}

extern LEVEL_BASE::KNOB<UInt32> g_knob_total_cores;
extern LEVEL_BASE::KNOB<UInt32> g_knob_num_process;
extern LEVEL_BASE::KNOB<bool> g_knob_simarch_has_shared_mem;
extern LEVEL_BASE::KNOB<std::string> g_knob_output_file;
extern LEVEL_BASE::KNOB<bool> g_knob_enable_performance_modeling;
extern LEVEL_BASE::KNOB<bool> g_knob_enable_dcache_modeling;
extern LEVEL_BASE::KNOB<bool> g_knob_enable_icache_modeling;
extern LEVEL_BASE::KNOB<bool> g_knob_enable_syscall_modeling;

void HandleArgs()
{
    cfg->set("general/total_cores", (int)g_knob_total_cores.Value());
    cfg->set("general/num_processes", (int)g_knob_num_process);
    cfg->set("general/enable_shared_mem", g_knob_simarch_has_shared_mem);
    cfg->set("general/enable_syscall_modeling", g_knob_simarch_has_shared_mem);
    cfg->set("general/enable_performance_modeling", g_knob_enable_performance_modeling);
    cfg->set("general/enable_dcache_modeling", g_knob_enable_dcache_modeling);
    cfg->set("general/enable_icache_modeling", g_knob_enable_icache_modeling);
    cfg->set("general/enable_syscall_modeling", g_knob_enable_syscall_modeling);
}

void ApplicationExit(int, void*)
{
   LOG_PRINT("Application exit.");
   Simulator::release();
   delete cfg;
}

// void ThreadStart(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, void *v)
// {
//    LOG_PRINT("Thread Start: %d", syscall(__NR_gettid));
// }

// void ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, void *v)
// {
//    LOG_PRINT("Thread Fini: %d", syscall(__NR_gettid));
// }

void SimSpawnThreadSpawner(CONTEXT *ctx, AFUNPTR fp_main)
{
   // Get the function for the thread spawner
   PIN_LockClient();
   AFUNPTR thread_spawner;
   IMG img = IMG_FindByAddress((ADDRINT)fp_main);
   RTN rtn = RTN_FindByName(img, "CarbonSpawnThreadSpawner");
   thread_spawner = RTN_Funptr(rtn);
   PIN_UnlockClient();

   // Get the address of the thread spawner
   int res;
   PIN_CallApplicationFunction(ctx,
         PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         thread_spawner,
         PIN_PARG(int), &res,
         PIN_PARG_END());

}

int SimMain(CONTEXT *ctx, AFUNPTR fp_main, int argc, char *argv[])
{
   ApplicationStart();

   SimSpawnThreadSpawner(ctx, fp_main);

   if (Config::getSingleton()->getCurrentProcessNum() == 0)
   {
      LOG_PRINT("Calling main()...");

      Sim()->getCoreManager()->initializeThread(0);

      // call main()
      int res;
      PIN_CallApplicationFunction(ctx,
                                  PIN_ThreadId(),
                                  CALLINGSTD_DEFAULT,
                                  fp_main,
                                  PIN_PARG(int), &res,
                                  PIN_PARG(int), argc,
                                  PIN_PARG(char**), argv,
                                  PIN_PARG_END());
   }
   else
   {
      LOG_PRINT("Waiting for main process to finish...");
      while (!Sim()->finished())
         usleep(100);
      LOG_PRINT("Finished!");
   }

   LOG_PRINT("Leaving SimMain...");

   return 0;
}

int main(int argc, char *argv[])
{
   // Global initialization
   PIN_InitSymbols();

   if (PIN_Init(argc,argv))
      return usage();


   cfg = new config::ConfigFile();
   cfg->load("./carbon_sim.cfg");

   // This sets items in the config accoring to
   // the general pin knobs
   HandleArgs();

   Simulator::setConfig(cfg);

   Simulator::allocate();
   Sim()->start();

   // Instrumentation
   LOG_PRINT("Start of instrumentation.");
   RTN_AddInstrumentFunction(routineCallback, 0);

   if(cfg->getBool("general/enable_syscall_modeling"))
   {
       PIN_AddSyscallEntryFunction(SyscallEntry, 0);
       PIN_AddSyscallExitFunction(SyscallExit, 0);
   }

//   PIN_AddThreadStartFunction(ThreadStart, 0);
//   PIN_AddThreadFiniFunction(ThreadFini, 0);
   PIN_AddFiniFunction(ApplicationExit, 0);

   // Just in case ... might not be strictly necessary
   Transport::getSingleton()->barrier();

   // Never returns
   LOG_PRINT("Running program...");
   PIN_StartProgram();

   return 0;
}
