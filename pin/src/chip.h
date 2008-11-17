// Jonathan Eastep and Harshad Kasture
//

#ifndef CHIP_H
#define CHIP_H

#include <iostream>
#include <fstream>
#include <map>

#include "pin.H"
#include "capi.h"
#include "core.h"
#include "ocache.h"
#include "dram_directory_entry.h"
#include "cache_state.h"
#include "address_home_lookup.h"
#include "perfmdl.h"
#include "syscall_model.h"
#include "mcp.h"

// external variables

// JME: not entirely sure why these are needed...
class Chip;
class Core;

extern Chip *g_chip;
extern MCP *g_MCP;
extern PIN_LOCK print_lock;

extern LEVEL_BASE::KNOB<string> g_knob_output_file;

// prototypes

// FIXME: if possible, these shouldn't be globals. Pin callbacks may need them to be. 

CAPI_return_t chipInit(int *rank);

CAPI_return_t chipRank(int *rank);

CAPI_return_t commRank(int *rank);

CAPI_return_t chipSendW(CAPI_endpoint_t sender, CAPI_endpoint_t receiver,
                        char *buffer, int size);

CAPI_return_t chipRecvW(CAPI_endpoint_t sender, CAPI_endpoint_t receiver,
                        char *buffer, int size);
//harshad should replace this -cpc
CAPI_return_t chipHackFinish(int my_rank);

//deal with locks in cout
CAPI_return_t chipPrint(string s);

CAPI_return_t chipDebugSetMemState(ADDRINT address, INT32 dram_address_home_id, DramDirectoryEntry::dstate_t dstate, CacheState::cstate_t cstate0, CacheState::cstate_t cstate1, vector<UINT32> sharers_list, char *d_data, char *c_data);

CAPI_return_t chipDebugAssertMemState(ADDRINT address, INT32 dram_address_home_id, DramDirectoryEntry::dstate_t dstate, CacheState::cstate_t cstate0, CacheState::cstate_t cstate1, vector<UINT32> sharers_list, char *d_data, char *c_data, string test_code, string error_code);


CAPI_return_t chipSetDramBoundaries(vector< pair<ADDRINT, ADDRINT> > addr_boundaries);

// Stupid Hack
CAPI_return_t chipAlias (ADDRINT address0, addr_t addrType, UINT32 num);
ADDRINT createAddress (UINT32 num, UINT32 coreId, bool pack1, bool pack2);
UINT32 log(UINT32);
// performance model wrappers

void perfModelRun(PerfModelIntervalStat *interval_stats);

void perfModelRun(PerfModelIntervalStat *interval_stats, REG *reads, 
                  UINT32 num_reads);

void perfModelRun(PerfModelIntervalStat *interval_stats, bool dcache_load_hit, 
                  REG *writes, UINT32 num_writes);

PerfModelIntervalStat** perfModelAnalyzeInterval(const string& parent_routine, 
                                                 const INS& start_ins, const INS& end_ins);

void perfModelLogICacheLoadAccess(PerfModelIntervalStat *stats, bool hit);
     
void perfModelLogDCacheStoreAccess(PerfModelIntervalStat *stats, bool hit);

void perfModelLogBranchPrediction(PerfModelIntervalStat *stats, bool correct);


// organic cache model wrappers

bool icacheRunLoadModel(ADDRINT i_addr, UINT32 size);

bool dcacheRunModel(CacheBase::AccessType access_type, ADDRINT d_addr, char* data_buffer, UINT32 size);
//TODO removed these because it's unneccsary
//shared memory doesn't care, but for legacy sake, i'm leaving them here for now
//bool dcacheRunLoadModel(ADDRINT d_addr, UINT32 size);
//bool dcacheRunStoreModel(ADDRINT d_addr, UINT32 size);

// syscall model wrappers
void syscallEnterRunModel(CONTEXT *ctx, SYSCALL_STANDARD syscall_standard);
void syscallExitRunModel(CONTEXT *ctx, SYSCALL_STANDARD syscall_standard);

// MCP server wrappers
void MCPRun();


// chip class

class Chip 
{

   // wrappers
   public:

      // messaging wrappers

      friend CAPI_return_t chipInit(int *rank); 
      friend CAPI_return_t chipRank(int *rank);
      
	  //FIXME: this is a hack function 
	  friend CAPI_return_t chipHackFinish(int my_rank);
	  
	  friend CAPI_return_t chipPrint(string s);
      
      friend CAPI_return_t commRank(int *rank);
      friend CAPI_return_t chipSendW(CAPI_endpoint_t sender, 
                                     CAPI_endpoint_t receiver, char *buffer, 
                                     int size);
      friend CAPI_return_t chipRecvW(CAPI_endpoint_t sender, 
                                     CAPI_endpoint_t receiver, char *buffer, 
                                     int size);

      // performance modeling wrappers
 
      friend void perfModelRun(PerfModelIntervalStat *interval_stats);
      friend void perfModelRun(PerfModelIntervalStat *interval_stats, REG *reads, 
                               UINT32 num_reads);
      friend void perfModelRun(PerfModelIntervalStat *interval_stats, bool dcache_load_hit, 
                               REG *writes, UINT32 num_writes);
      friend PerfModelIntervalStat** perfModelAnalyzeInterval(const string& parent_routine, 
                                                              const INS& start_ins, 
                                                              const INS& end_ins);
      friend void perfModelLogICacheLoadAccess(PerfModelIntervalStat *stats, bool hit);
      friend void perfModelLogDCacheStoreAccess(PerfModelIntervalStat *stats, bool hit);
      friend void perfModelLogBranchPrediction(PerfModelIntervalStat *stats, bool correct);      

      // syscall modeling wrapper
      friend void syscallEnterRunModel(CONTEXT *ctx, SYSCALL_STANDARD syscall_standard);
      friend void syscallExitRunModel(CONTEXT *ctx, SYSCALL_STANDARD syscall_standard);

      
      // organic cache modeling wrappers
      friend bool icacheRunLoadModel(ADDRINT i_addr, UINT32 size);
      friend bool dcacheRunModel(CacheBase::AccessType access_type, ADDRINT d_addr, char* data_buffer, UINT32 data_size);
		//TODO deprecate these two bottom functions
      friend bool dcacheRunLoadModel(ADDRINT d_addr, UINT32 size);
      friend bool dcacheRunStoreModel(ADDRINT d_addr, UINT32 size);

		// FIXME: A hack for testing purposes
		friend CAPI_return_t chipAlias(ADDRINT address0, addr_t addType, UINT32 num);
		friend bool isAliasEnabled(void);

   private:

      int num_modules;

      UINT64 *proc_time;

      // tid_map takes core # to pin thread id
      // core_map takes pin thread id to core # (it's the reverse map)
      THREADID *tid_map;
      map<THREADID, int> core_map;
      int prev_rank;
      PIN_LOCK maps_lock;
      PIN_LOCK dcache_lock;
      
      Core *core;

		//debugging crap: 
		PIN_LOCK global_lock;
		//chipFinishHack (remove later)
		//i'm tracking which cores have finished
		//running the user program. when all cores
		//have checked in, we can tell them to quit the program.
		bool* finished_cores;

   public:

      //debugging crap
		void getGlobalLock() { GetLock(&global_lock, 1); }
		void releaseGlobalLock() { ReleaseLock(&global_lock); }

		//**************
		

		// FIXME: This is strictly a hack for testing data storage
		bool aliasEnable;
		std::map<ADDRINT,ADDRINT> aliasMap;
		//////////////////////////////////////////////////////////


      UINT64 getProcTime(int module) { assert(module < num_modules); return proc_time[module]; }

      void setProcTime(int module, unsigned long long new_time) 
      { assert(module < num_modules); proc_time[module] = new_time; }

      int getNumModules() { return num_modules; }

      Chip(int num_mods);

      void fini(int code, void *v);

		//input parameters: 
		//an address to set the conditions for
		//a dram vector, with a pair for the id's of the dram directories to set, and the value to set it to
		//a cache vector, with a pair for the id's of the caches to set, and the value to set it to
		void debugSetInitialMemConditions (vector<ADDRINT>& address_vector, 
		  											  vector< pair<INT32, DramDirectoryEntry::dstate_t> >& dram_vector, vector<vector<UINT32> >& sharers_list_vector, 
													  vector< vector< pair<INT32, CacheState::cstate_t> > >& cache_vector, 
		  											  vector<char*>& d_data_vector, 
													  vector<char*>& c_data_vector);
		bool debugAssertMemConditions (vector<ADDRINT>& address_vector, 
		  										 vector< pair<INT32, DramDirectoryEntry::dstate_t> >& dram_vector, vector<vector<UINT32> >& sharers_list_vector, 
												 vector< vector< pair<INT32, CacheState::cstate_t> > >& cache_vector, 
		  										 vector<char*>& d_data_vector, 
												 vector<char*>& c_data_vector,
												 string test_code, string error_string);
		
		/*
		void setDramBoundaries(vector< pair<ADDRINT, ADDRINT> > addr_boundaries);
		*/

		void getDCacheModelLock(int rank);
		void releaseDCacheModelLock(int rank);

};

#endif
