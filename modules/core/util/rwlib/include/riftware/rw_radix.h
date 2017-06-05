/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_radix.h
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 05/5/2014
 * @header 
 */

#ifndef RW_RADIX_H
#define RW_RADIX_H

#include "rw_ip.h"

__BEGIN_DECLS

typedef struct rw_radix_node_s
{
  rw_ip_prefix_t         ip;           /**<network ip address */
  struct rw_radix_trie_s *trie;        /**< Back pointer to the radix trie */
  struct rw_radix_node_s *parent;      /**< Back pointer to the parent */
  struct rw_radix_node_s *left_child;  /**< left child */
  struct rw_radix_node_s *right_child; /**< right child */
  void                   *leaf_data;   /**< leaf data */
  uint32_t               refcnt;       /**refcnt for the node */
} rw_radix_node_t;

typedef struct rw_radix_trie_s
{
  rw_radix_node_t *head; /**< top node*/
  int num_leaf;
} rw_radix_trie_t;

static inline void rw_radix_attach_leaf_data(rw_radix_node_t *node,
                                      void *data)
{
  if (node->leaf_data){
    node->trie->num_leaf--;
    node->leaf_data = NULL;
  }
  if (data){
    node->leaf_data = data;
    node->trie->num_leaf++;
  }
}

rw_radix_trie_t *rw_radix_trie_init (void);
void rw_radix_trie_free(rw_radix_trie_t *);
void rw_radix_node_delete (rw_radix_node_t *node);
rw_radix_node_t *rw_radix_head (rw_radix_trie_t *);
rw_radix_node_t *rw_radix_node_next (rw_radix_node_t *);
rw_radix_node_t *rw_radix_node_lpm (rw_radix_trie_t *,
                                    rw_ip_prefix_t *);
rw_radix_node_t *rw_radix_node_insert (rw_radix_trie_t *trie,
				       rw_ip_prefix_t *p);
rw_radix_node_t *rw_radix_node_lookup(rw_radix_trie_t *trie,
				      rw_ip_prefix_t *p);
void rw_radix_change_order(rw_ip_prefix_t *prefix);

__END_DECLS

#endif /* RW_RADIX_H */
