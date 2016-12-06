
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *
 */

#ifndef __RWLOGD_UTILS_H__
#define __RWLOGD_UTILS_H__
#include <iostream>
#include <algorithm>    // std::sort
#include <deque>       // std::deque
#include <map>
#define RW_LKUP_SZ 2048

typedef struct {
  uint32_t inserts;
  uint32_t  removal_success;
  uint32_t removal_failure;
  uint32_t cur_size;
  uint32_t max_size;
}ring_buffer_stats_t;

template <class  T>
class ring_buffer
{
  typedef bool (*comfnc)(const T a, const T b);
  private:
    std::deque<T> elems_;
    comfnc comp_;

  public:
    uint32_t  inserts_;
    uint32_t removal_success_;
    uint32_t removal_failure_;
    uint32_t max_size_;
    uint32_t cur_size_;
    uint32_t first_elem_removal_;
    ring_buffer(size_t sz, comfnc comp) {
      max_size_   = sz;
      cur_size_   = 0;
      inserts_ = 0;
      removal_success_ = 0;
      removal_failure_ = 0;
      first_elem_removal_ = 0;
      comp_ = comp;
      //elems_.reserve(sz);
    }

    ~ring_buffer() {
      elems_.clear();
    }

    int add(const T obj) {
      typename std::deque<T>::iterator start, position;
#if 0
      if(cur_size_ && comp_(obj,*(elems_.end()-1)) == FALSE) {
        position = elems_.end();
      }
#endif
      position = std::lower_bound(elems_.begin(), elems_.end(), obj, comp_);
      cur_size_++;
      if(cur_size_ > max_size_) { max_size_ = cur_size_; }
      elems_.insert(position,obj);
      inserts_++;  
      return (std::distance(elems_.begin(),position));
    }

    int reverse_add(const T obj) {
      typename std::deque<T>::iterator  position;

      if(std::binary_search(elems_.begin(), elems_.end(), obj, comp_) == TRUE) {
        return 0;
      }
      position = std::lower_bound(elems_.begin(), elems_.end(), obj, comp_);
      auto offset = std::distance(elems_.begin(),position);
      if(offset >= max_size_) {return offset;}
      if (cur_size_ == max_size_) elems_.erase(elems_.end()-1); else cur_size_++;
      elems_.insert(position,obj);
      return (std::distance(elems_.begin(),position));
    }

    void remove(const T obj,comfnc comp_match) {
      typename std::deque<T>::iterator position;
    
      if(comp_match(obj,*elems_.begin()) == TRUE) {
        elems_.erase(elems_.begin());
        cur_size_--;
        first_elem_removal_++;
        removal_success_++;
      }
      else {
        position = std::lower_bound(elems_.begin(), elems_.end(), obj, comp_);
        if(comp_match(obj,*position) == TRUE) {
          elems_.erase(position);
          cur_size_--;
          removal_success_++;
        }
        else {
          removal_failure_++;
        }
      }
    }

   void clear() 
   {
     removal_success_ += elems_.size();
     elems_.clear();
     cur_size_ = elems_.size();
     return;
   }

    T get(int &pos)
    {
      RW_ASSERT(pos>=0);
      if((size_t)pos<cur_size_)
      {
        return elems_[pos];
      }
      else {
        return (0);
      }
    }

    int get_position(T obj)
    {
      typename std::deque<T>::iterator position;
      position = std::lower_bound(elems_.begin(), elems_.end(), obj, comp_);
      if (position == elems_.end()) {
        return (-1);
      }
      return (std::distance(elems_.begin(),position));
    }

    int get_last_position()
    {
        return cur_size_-1;
    }

    T begin()
    {
      return elems_.begin();
    }
    T end()
    {
      return elems_.end();
    }
    void rwlog_dump_info(int verbose)
    {
      printf("Log Ring Info \n");
      printf("--------------------\n");
      printf("max_size_ %u\n", max_size_);
      printf("cur_size_ %u\n",cur_size_);
      printf("inserts_ %u, \n",inserts_);
      printf("remvoval success: %u, removal failure: %u\n",removal_success_,removal_failure_);
      printf("First elem removal success %u\n",first_elem_removal_);
      if(verbose<5)
      {
        return;
      }
      typename std::deque<T>::iterator it;
      print_hdr((T)0);
      for (it=elems_.begin(); it!=elems_.end(); ++it) {
        print(*it);
      }
    }
};


template  <class T1, class T2>
class rw_cache {
 private:
  size_t _max_size;
  uint64_t cache_lookup_called;
  uint64_t cache_add_called;
  uint64_t empty_cache_called;
  uint64_t cache_entry_overwritten;
 public:
  std::map<T1,T2> _cachemap; // Binary Search
  std::list<T1> _cacheentries; // list
  using cacheiter = typename std::map<T1,T2>::const_iterator;
  rw_cache(size_t max_size)
  {
    _max_size = max_size;
    cache_lookup_called =0;
    cache_add_called =0;
    empty_cache_called =0;
    cache_entry_overwritten =0;
  }
  ~rw_cache()
  {
    empty_cache();
  }
  T2 cache_lookup(T1 key)
  {
    cache_lookup_called++;
    RW_ASSERT(_cacheentries.size()<=_max_size);
    RW_ASSERT(_cachemap.size()<=_max_size);
    cacheiter it = _cachemap.find(key);
    if (it!=_cachemap.end())
    {
      _cacheentries.remove(key);
      _cacheentries.push_front(key);
      return it->second;
    }
    return NULL;
  }
  void cache_add(T1 key, T2 entry)
  {
    cache_add_called++;
    if(_cachemap.size() >=_max_size)
    {
      T1 rm_key = _cacheentries.back();
      _cachemap.erase(rm_key);
      _cacheentries.remove(rm_key);
      cache_entry_overwritten++;
    }
    _cachemap[key] = entry;
    _cacheentries.push_front(key);
  }
  void empty_cache()
  {
    empty_cache_called++;
    _cachemap.clear();
    _cacheentries.clear();
  }
  void print()
  {
    printf("RW_CACHE: stats\n");
    printf("\tcache_lookup_called: %lu\n",cache_lookup_called);
    printf("\tcache_add_called: %lu\n",cache_add_called);
    printf("\tempty_cache_called: %lu\n",empty_cache_called);
    printf("\tcache_entry_overwritten: %lu\n",cache_entry_overwritten);
  }
};

#endif /*__RWLOGD_UTILS_H__*/

