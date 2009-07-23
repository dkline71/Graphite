#include "cache_set.h"

CacheSetRoundRobin::CacheSetRoundRobin(UInt32 associativity, UInt32 blocksize) :
   CacheSet(associativity, blocksize)
{
   m_replacement_index = m_associativity - 1;
}

CacheSetRoundRobin::~CacheSetRoundRobin()
{}

UInt32
CacheSetRoundRobin::getReplacementIndex()
{
   UInt32 curr_replacement_index = m_replacement_index;
   m_replacement_index = (m_replacement_index == 0) ? (m_associativity-1) : (m_replacement_index-1);
   return curr_replacement_index; 
}

void
CacheSetRoundRobin::updateReplacementIndex(UInt32 accessed_index)
{
   return;
}
