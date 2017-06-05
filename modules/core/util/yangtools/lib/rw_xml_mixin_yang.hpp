/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_xml_mixin_yang.hpp
 * @author Tom Seidenberg
 * @date 2014/07/24
 * @brief XMLNode mixin for containing the schema details.
 */

#ifndef RW_XML_MIXIN_YANG_HPP_
#define RW_XML_MIXIN_YANG_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <rw_xml.h>
#include <string>
#include <utility>

/**
 * @addtogroup RW-XML-LIBRARY
 * @{
 */

namespace rw_yang {

/**
 * Convenience type for XMLNode construction with XMLNodeMixinYang<>.
 * Used with a concrete XMLNode constructor invocation like this:
 *
 *     XMLNode* node = new XMLNodeConcrete(mixin_ynode{ynode});
 */
struct mixin_ynode
{
  /// The natural yang schema node for XMLNodeMixinYang construction
  YangNode* ynode;
};

/**
 * Convenience type for XMLNode construction with XMLNodeMixinYang<>.
 * Used with a concrete XMLNode constructor invocation like this:
 *
 *     XMLNode* node = new XMLNodeConcrete(mixin_reroot{reroot});
 */
struct mixin_reroot
{
  /// The rerooted yang schema node for XMLNodeMixinYang construction
  YangNode* reroot;
};



/// XML Node YangModel Mixin
/**
 * This class template is a mixin that implements the rw_yang::XMLNode
 * yang shema APIs using a raw YangNode pointer.  This implementation
 * can be shared by multiple concrete RW.XML classes by virtue of the
 * mixin pattern.
 *
 * To use this template in a concrete RW.XML node, use the following
 * definition style for your class:
 *
 *     class XMLNodeConcrete
 *     : public XMLNodeMixinYang<XMLNode>
 *     {
 *       ...
 *     };
 *
 * To use multiple mixins, they can be chained by using one mixin as
 * the template parameter for another mixin.  Where possible, you
 * should try to use the same inner orderings as existing concrete
 * RW.XML classes, so that they can share template instantioations,
 * although it is not required.  Although mixins should not implement
 * the same APIs (that would not truly follow the mixin pattern), if
 * two mixins did implement the same method, be sure that the one you
 * want to keep is the outer-most template.
 *
 *     class XMLNodeConcrete
 *     : public XMLNodeMixinYang<XMLNodeMixinOther<XMLNode>>
 *     {
 *       ...
 *     };
 *
 * A key requirement of XMLNode mixins is that they all take well-known
 * typed arguments, and that all mixins take mutually distinct types.
 * This requirement allows template argument deduction to formulate the
 * correct constructor as needed, including taking no arguments at all.
 *
 * When using XMLNode mixins, the inner-most template paramter must
 * always be XMLNode, because XMLNode defines the complete interface.
 *
 * @ref <http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.103.144>
 * @implements rw_yang::XMLNode
 */
template <class mixwith_t>
class XMLNodeMixinYang
: public mixwith_t
{
 public:
  typedef mixwith_t XMLNodeMixinYang_mixwith_t;

 public:
  /**
   * Constructor for a known schema node.  Even if ynode is nullptr, this
   * API will set known_schema_ to true!
   */
  template <typename... more_args_ts>
  explicit XMLNodeMixinYang(
    /// The natural yang node initializer.
    mixin_ynode&& ynode,

    /// More constructor arguments, for the base classes.  Possibly none.
    more_args_ts&&... more_args_vs
  )
  : mixwith_t(std::forward<more_args_ts>(more_args_vs)...),
    ynode_(ynode.ynode),
    known_schema_(true)
  {}

  /**
   * Constructor for a re-rooted schema node.  Has no effect on the
   * natural schema node.
   */
  template <typename... more_args_ts>
  explicit XMLNodeMixinYang(
    /// The re-root yang node initializer.
    mixin_reroot&& reroot,

    /// More constructor arguments, for the base classes.  Possibly none.
    more_args_ts&&... more_args_vs
  )
  : mixwith_t(std::forward<more_args_ts>(more_args_vs)...),
    reroot_ynode_(reroot.reroot)
  {}

  /**
   * Default forwarding constructor.  This template matches when none
   * of the others match.
   */
  template <typename... more_args_ts>
  explicit XMLNodeMixinYang(
    /// The natural yang node initializer.
    mixin_ynode&& ynode,

    /// The re-root yang node initializer.
    mixin_reroot&& reroot,

    /// More constructor arguments, for the base classes.  Possibly none.
    more_args_ts&&... more_args_vs
  )
  : mixwith_t(std::forward<more_args_ts>(more_args_vs)...),
    ynode_(ynode.ynode),
    reroot_ynode_(reroot.reroot),
    known_schema_(true)
  {}

  /**
   * Default forwarding constructor.  This template matches when none
   * of the others match.
   */
  template <typename... more_args_ts>
  explicit XMLNodeMixinYang(
    /// More constructor arguments, for the base classes.  Possibly none.
    more_args_ts&&... more_args_vs
  )
  : mixwith_t(std::forward<more_args_ts>(more_args_vs)...)
  {}


 protected:
  /// Trivial protected destructor - see XMLNodeMixinYang::obj_release().
  virtual ~XMLNodeMixinYang()
  {}

  // Cannot copy
  XMLNodeMixinYang(const XMLNodeMixinYang &) = delete;
  XMLNodeMixinYang& operator = (const XMLNodeMixinYang&) = delete;

 public:

  // Base class interfaces implemented by the mixin
  bool known_yang_node() override;
  YangNode* get_yang_node() override;
  void set_yang_node(YangNode* yang_node) override;
  YangNode* get_reroot_yang_node() override;
  void set_reroot_yang_node(YangNode* reroot_yang_node) override;
  YangNode* get_descend_yang_node() override;


 protected:

  /**
   * The yang schema node that defines this XML node, from the point
   * of view of the parent.  See known_schema_ to determine if this
   * pointer it valid.
   */
  YangNode* ynode_ = nullptr;

  /**
   * If ynode_ is an anyxml node, or this is a non-schema XML node,
   * this yang schema node provides a new schema context for
   * descending further in the XML document.  See known_schema_ to
   * determine if this pointer it valid.
   */
  YangNode* reroot_ynode_ = nullptr;

  /**
   * The node's place in the shema has been determined.  When true, a
   * previous attempt to map the node to the yang schema was
   * performed, and ynode_ should be assumed to be correct.  If true
   * and ynode_ is nullptr, no matching schema node was found.
   */
  bool known_schema_ = false; // : 1 = false; ATTN: Does not allow in-class initialization of bitfield.  Fix with c++14
};


template <class mixwith_t>
bool XMLNodeMixinYang<mixwith_t>::known_yang_node()
{
  return known_schema_;
}


template <class mixwith_t>
YangNode* XMLNodeMixinYang<mixwith_t>::get_yang_node()
{
  if (known_schema_) {
    return ynode_;
  }

  // Lookup this node in the parent node's schema, if possible
  if (this == this->get_document().get_root_node()) {
    std::string local_name = this->get_local_name();
    std::string ns = this->get_name_space();
    YangNode* root_ynode = this->get_manager().get_yang_model()->get_root_node();
    ynode_ = root_ynode->search_child( local_name.c_str(), ns.c_str() );
  } else {
    XMLNode* parent_xnode = this->get_parent();
    if (parent_xnode) {
      YangNode* parent_ynode = parent_xnode->get_descend_yang_node();
      if (parent_ynode) {
        std::string local_name = this->get_local_name();
        std::string ns = this->get_name_space();
        if (ns.length()) {
          ynode_ = parent_ynode->search_child( local_name.c_str(), ns.c_str() );
        } else {
          ynode_ = parent_ynode->search_child( local_name.c_str(), nullptr );
        }
      }
    }
  }
  known_schema_ = true;
  return ynode_;
}

template <class mixwith_t>
void XMLNodeMixinYang<mixwith_t>::set_yang_node(
  YangNode* yang_node)
{
  RW_ASSERT(ynode_ == nullptr || ynode_ == yang_node);
  ynode_ = yang_node;
  known_schema_ = true;
}

template <class mixwith_t>
YangNode* XMLNodeMixinYang<mixwith_t>::get_reroot_yang_node()
{
  return reroot_ynode_;
}

template <class mixwith_t>
void XMLNodeMixinYang<mixwith_t>::set_reroot_yang_node(
  YangNode* reroot_yang_node)
{
  reroot_ynode_ = reroot_yang_node;
}

template <class mixwith_t>
YangNode* XMLNodeMixinYang<mixwith_t>::get_descend_yang_node()
{
  YangNode* retval = get_reroot_yang_node();

  if (retval) {
    return retval;
  }

  return get_yang_node();
}

}

/** @} */

#endif // RW_XML_MIXIN_YANG_HPP_
