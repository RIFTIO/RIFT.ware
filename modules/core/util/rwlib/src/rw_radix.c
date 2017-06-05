
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

/**
 * @file rw_radix.c
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 05/01/2014
 * @brief radix tree 
 * 
 * @details 
 *
 */
#include "rwtypes.h"
#include "rw_radix.h"


#define NBPB 8 /*Number of bits per byte*/

//AKKI to be moved

void rw_radix_change_order(rw_ip_prefix_t *prefix)
{
  if (prefix->ip_v == AF_INET){
    prefix->u.v4.addr = htonl(prefix->u.v4.addr);
  }else{
    int i;
    for (i = 0; i < 4; i++){
      prefix->u.v6.addr[i] = htonl(prefix->u.v6.addr[i]);
    }
  }
}

const uint8_t rw_radix_maskbit[] =
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};


static inline uint32_t
rw_radix_prefix_bit (uint8_t *prefix, uint8_t masklen)
{
  uint32_t octet = masklen / 8;
  uint32_t bit  = 7 - (masklen % 8);

  return (prefix[octet] >> bit) & 1;
}

/**
 * This fucntino is called to check if the prefix (second argument) matches the a 
 * certain prefix (first agument)
 */
int
rw_radix_prefix_match (rw_ip_prefix_t *r, rw_ip_prefix_t *p)
{
  uint8_t *route;
  uint8_t *prefix;
  int num_octet;
  int bit_position;
  
  RW_ASSERT(r);
  RW_ASSERT(p);
  
  if (r->masklen > p->masklen){
    return 0;
  }
  route   = &r->u.data;
  prefix  = &p->u.data;
  
  num_octet = r->masklen / NBPB;
  bit_position =  r->masklen % (NBPB);

  if (bit_position){
    if (rw_radix_maskbit[bit_position] & (route[num_octet] ^ prefix[num_octet]))
      return 0;
  }
  
  while(num_octet--){
    if (route[num_octet] != prefix[num_octet]){
      return 0;
    }
  }
  
  return 1;
}
//AKKI to be moved



static inline void
rw_radix_set_child (rw_radix_node_t *node, rw_radix_node_t *new)
{
  unsigned int bit = rw_radix_prefix_bit (&new->ip.u.data, node->ip.masklen);

  if (bit){
    node->right_child = new;
  }else{
    node->left_child = new;
  }
  new->parent = node;
}

/**
 * This fucntion is called to get the internal node prefix
 */
static void
rw_radix_node_internal (rw_ip_prefix_t *n, rw_ip_prefix_t *p, rw_ip_prefix_t *new)
{
  int i;
  uint8_t diff;
  uint8_t mask;
  uint8_t *np = &n->u.data;
  uint8_t *pp = &p->u.data;
  uint8_t *newp = &new->u.data;

  new->ip_v = n->ip_v;
  
  for (i = 0; i < p->masklen / 8; i++)
    {
      if (np[i] == pp[i])
	newp[i] = np[i];
      else
	break;
    }

  new->masklen = i * 8;

  if (new->masklen != p->masklen)
    {
      diff = np[i] ^ pp[i];
      mask = 0x80;
      while (new->masklen < p->masklen && !(mask & diff))
	{
	  mask >>= 1;
	  new->masklen++;
	}
      newp[i] = np[i] & rw_radix_maskbit[new->masklen % 8];
    }
}

/**
 * This fucntion is called to find the longest prefix match node
 */
rw_radix_node_t *
rw_radix_node_lpm(rw_radix_trie_t *trie, rw_ip_prefix_t *key)
{
  rw_radix_node_t *node;
  rw_radix_node_t *matched;
  rw_ip_prefix_t radix_prefix, *p;

  //AKKi users today are using prefix in host order
  rw_ip_prefix_copy(&radix_prefix, key);
  rw_radix_change_order(&radix_prefix);
  p = &radix_prefix;
  
  matched = NULL;
  node = trie->head;

  while (node &&
         node->ip.masklen <= p->masklen && 
	 rw_radix_prefix_match (&node->ip, p))
  {
    if (node->leaf_data){
	matched = node;
    }
    if (rw_radix_prefix_bit(&p->u.data, node->ip.masklen))
      node = node->right_child;
    else
      node = node->left_child;
  }
  
  return matched;
}


/**
 * This fucntion is called to find the longest prefix match node
 */
rw_radix_node_t *
rw_radix_node_lookup(rw_radix_trie_t *trie, rw_ip_prefix_t *key)
{
  rw_radix_node_t *node;
  rw_ip_prefix_t radix_prefix, *p;

  //AKKi users today are using prefix in host order
  rw_ip_prefix_copy(&radix_prefix, key);
  rw_radix_change_order(&radix_prefix);
  p = &radix_prefix;
  
  node = trie->head;

  while (node &&
         node->ip.masklen <= p->masklen && 
	 rw_radix_prefix_match (&node->ip, p))
  {
    if (node->leaf_data &&
        (node->ip.masklen == p->masklen)){
      return node;
    }
    if (rw_radix_prefix_bit(&p->u.data, node->ip.masklen))
      node = node->right_child;
    else
      node = node->left_child;
  }
  
  return NULL;
}


static rw_radix_node_t*
rw_radix_node_attach(rw_radix_trie_t *trie, rw_radix_node_t *parent,
                     rw_radix_node_t *child,
                     rw_ip_prefix_t *p)
{
  rw_radix_node_t *new;
  
  new = RW_MALLOC0(sizeof (*new));
  
  rw_ip_prefix_copy (&new->ip, p);
  new->trie = trie;
  if (child)
    rw_radix_set_child (new, child);
  
  if (parent){
    rw_radix_set_child (parent, new);
  }else{
    trie->head = new;
  }

  return new;
}

rw_radix_node_t *
rw_radix_node_insert (rw_radix_trie_t *trie, rw_ip_prefix_t *key)
{
  rw_radix_node_t *new;
  rw_radix_node_t *node = trie->head;
  rw_radix_node_t *match = NULL;
  rw_ip_prefix_t  internal_prefix;
  rw_ip_prefix_t radix_prefix, *p;

  //AKKi users today are using prefix in host order
  rw_ip_prefix_copy(&radix_prefix, key);
  rw_radix_change_order(&radix_prefix);
  p = &radix_prefix;
  
  while (node && node->ip.masklen <= p->masklen && 
	 rw_radix_prefix_match (&node->ip, p))
    {
      if (node->ip.masklen == p->masklen){
	  return node;
      }
      match = node;
      if (rw_radix_prefix_bit(&p->u.data, node->ip.masklen)){
        node = node->right_child;
      }else{
        node = node->left_child;
      }
    }
  
  if (node) {
    rw_radix_node_internal (&node->ip, p, &internal_prefix);
    new = rw_radix_node_attach(trie, match, node, &internal_prefix);
    if (internal_prefix.masklen != p->masklen)
    {
      new = rw_radix_node_attach(trie, new, NULL, p);
    }
  } else {
    new = rw_radix_node_attach(trie, match, NULL, p);
  }
  
  return new;
}

/**
 * This function is called to delete a node in the trie. It will delete any internal nodes
 * if any. If the node has both the children then such a node cannot be deleted. 
 * @param[in] node to be deleted
 * @return void
 */
void
rw_radix_node_delete (rw_radix_node_t *node)
{
  rw_radix_node_t *child;
  rw_radix_node_t *parent;
  
  if (node->left_child || node->right_child){
    return;
  }
  
  if (node->left_child)
    child = node->left_child;
  else
    child = node->right_child;

  if (child) {
    child->parent = node->parent;
  }
  
  if (node->parent){
    if (node->parent->left_child == node){
	node->parent->left_child = child;
    }else{
	node->parent->right_child = child;
    }
    parent = node->parent;
    RW_FREE(node);
    if (!(parent->left_child || parent->right_child || parent->leaf_data)){
      rw_radix_node_delete (parent);
    }
  }else{
    node->trie->head = child;
    RW_FREE(node);
  }
}


/**
 * This function is called to get the next node in the trie given the current node
 * It is used to traverse the trie
 * @param[in] node - current node
 * @return next node
 */
rw_radix_node_t *
rw_radix_node_next (rw_radix_node_t *node)
{
  RW_ASSERT(node);
  
  if (node->left_child){
    return node->left_child;
  }
  
  if (node->right_child){
    return node->right_child;
  }
  
  while (node->parent){
    if ((node->parent->left_child == node) && (node->parent->right_child))
    {
      return node->parent->right_child;
    }
    node = node->parent;
  }
  
  return NULL;
}


/**
 * This function is called to initialize the radix tree. It just allocates the memory
 * @param void
 * @return ptr to the radix tree
 */
rw_radix_trie_t *
rw_radix_trie_init(void)
{
  return RW_MALLOC0(sizeof (rw_radix_trie_t));
}

/**
 * This function is called to get the head of the trie
 */
rw_radix_node_t *rw_radix_head (rw_radix_trie_t *trie)
{
  return trie->head;
}

/**
 * This function frees all the nodes in the radix trie
 * @param[in] pointer to the radix trie
 * @return none
 */
void rw_radix_trie_free (rw_radix_trie_t *rt)
{
  rw_radix_node_t *tmp_node;
  rw_radix_node_t *node;
 
  if (rt == NULL)
    return;

  node = rt->head;

  while (node){
    if (node->left_child){
	  node = node->left_child;
	  continue;
    }
    
    if (node->right_child){
      node = node->right_child;
      continue;
    }
    
    tmp_node = node;
    node = node->parent;
    
    if (node != NULL)
    {
      if (node->left_child == tmp_node)
        node->left_child = NULL;
      else
        node->right_child = NULL;
      
      RW_FREE(tmp_node);
    }
    else
    {
      RW_FREE(tmp_node);
      break;
    }
  }
  
  RW_FREE (rt);
  return;
}

#if 0
/**
 * This fucntion is called to get the internal node prefix
 */
static void
rw_radix_node_internal (rw_ip_prefix_t *n, rw_ip_prefix_t *p, rw_ip_prefix_t *new)
{
  uint32_t diff;
  int i;
  
  rw_ip_prefix_copy(new, n);
  
  if (new->ip_v == AF_INET){
    diff = (n->u.v4.addr ^ p->u.v4.addr);
    if (diff){
      i = 1;
      while ((diff << i) >> (32-1) == 0)
        i++;
      new->masklen = i;
    }
  }else{
    int iter;
    new->masklen = 0;
    for (iter = 0; iter < 4; iter++){
      diff = (n->u.v6.addr[iter] ^ p->u.v6.addr[iter]);
      if (diff){
        i = 1;
        while ((diff << i) >> (32-1) == 0)
          i++;
      }else{
        i += 32;
      }
      new->masklen  += i;
    }
  }
  rw_ip_prefix_apply_mask(new);
}
static inline uint32_t
rw_ip_prefix_bit (rw_ip_prefix_t *prefix, uint8_t masklen)
{
  if (prefix->ip_v == AF_INET){
    return ((1 << (32 -(masklen+1))) & prefix->u.v4.addr);
  }else{
    return ((1 << (32 -((masklen+1)&0x1f))) & prefix->u.v6.addr[masklen/32]);
  }
}

#endif
