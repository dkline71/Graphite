#ifndef DRAM_DIRECTORY_H
#define DRAM_DIRECTORY_H

#include "pin.H"
#include "dram_directory_entry.h"
#include <map>

class DramDirectory
{
 private:
   UINT32 num_lines;
   unsigned int bytes_per_cache_line;
   UINT32 number_of_cores;
   //key dram entries on cache_line (assumes cache_line is 1:1 to dram memory lines)
   std::map<UINT32, DramDirectoryEntry*> dram_directory_entries;
   UINT32 dram_id;

public:
   DramDirectory(UINT32 num_lines, UINT32 bytes_per_cache_line, UINT32 dram_id_arg, UINT32 num_of_cores);
   virtual ~DramDirectory();
   DramDirectoryEntry* getEntry(ADDRINT address);
   void setNumberOfLines(UINT32 number_of_lines) { num_lines = number_of_lines; }

   void debugSetDramState(ADDRINT addr, DramDirectoryEntry::dstate_t dstate, vector<UINT32> sharers_list);
	bool debugAssertDramState(ADDRINT addr, DramDirectoryEntry::dstate_t dstate, vector<UINT32> sharers_list);

   //for debug purposes
   void print();
};

#endif
