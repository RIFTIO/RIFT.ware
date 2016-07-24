
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <string.h>
#include <map>
#include <openssl/sha.h>

/* IMPORTANT NOTE: 
 * This file only contains wrapper code. Consistent Hash needs to be implemented
 * */

typedef std::map<std::string,uint64_t> consistent_ring_t;

extern "C"
{

void *rwlogd_create_consistent_hash_ring(unsigned int replicas) 
{
  return NULL;
}

void rwlogd_delete_consistent_hash_ring(void *consistent_ring) 
{
}

void rwlogd_add_node_to_consistent_hash_ring(void *consistent_ring,char *node_name) 
{
}

void rwlogd_remove_node_from_consistent_hash_ring(void *consistent_ring,char *node_name) 
{
}
          
char * rwlogd_get_node_from_consistent_hash_ring_for_key(void *consistent_ring,uint64_t key)
{
  return NULL;
}

void rwlogd_display_consistent_hash_ring(void *consistent_ring) 
{
}


}

