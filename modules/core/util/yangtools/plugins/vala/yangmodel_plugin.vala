/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file yangmodel_plugin.vala
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 05/13/2014
 * @brief Vala interface for YangModel
 * 
 * @abstract
 * This vala specification defines the interface API for acceing YangModel routines
 */

using RwYang;
using RwTypes;
using RwYangPb;
using RwKeyspec;
using RwSchemaProto;

namespace YangmodelPlugin {
  public interface Model: GLib.Object {
    public abstract uint64 alloc();
    public abstract void free(uint64 model);
    public abstract uint64 load_module(uint64 model, string module_name);
    public abstract RwTypes.RwStatus load_schema_ypbc(uint64 model, RwYangPb.Schema schema);
    public abstract uint64 get_root_node(uint64 model);
    public abstract uint64 search_module_ns(uint64 model, string ns);
    public abstract uint64 search_module_name_rev(uint64 model, string name, string revision);
    public abstract uint64 search_module_ypbc(uint64 model, uint64 ypbc_mod);
    public abstract uint64 get_first_module(uint64 model);
    public abstract uint64 load_and_get_schema(string schema_name);
    /*
    public abstract void set_log_level(uint64 model, YangEnums.LogLevel level);
    public abstract YangEnums.LogLevel log_level(uint64 model);
    */
  }

  public interface Module: GLib.Object {
    public abstract void location(uint64 ymod, out string str /* Allocated string, owned by the caller! */);
    public abstract unowned string? description(uint64 ymod);
    public abstract unowned string? name(uint64 ymod);
    public abstract unowned string? prefix(uint64 ymod);
    public abstract unowned string? ns(uint64 ymod);
    public abstract uint64 next_module(uint64 ymod);
    public abstract bool loaded_explicitly(uint64 ymod);
    public abstract uint64 first_node(uint64 ymod);
    public abstract uint64 first_extension(uint64 ymod);
    public abstract uint64 first_augment(uint64 ymod);
  }

  public interface Node: GLib.Object {
    public abstract void location(uint64 ynode, out string str /* Allocated string, owned by the caller! */);
    public abstract unowned string? description(uint64 ynode);
    public abstract unowned string? help_short(uint64 ynode);
    public abstract unowned string? help_full(uint64 ynode);
    public abstract unowned string? name(uint64 ynode);
    public abstract unowned string? prefix(uint64 ynode);
    public abstract unowned string? ns(uint64 ynode);
    public abstract unowned string? default_value(uint64 ynode);
    public abstract uint32 max_elements(uint64 ynode);
    /*
    public abstract YangEnums.StmtType stmt_type(uint64 ynode);
    */
    public abstract bool is_config(uint64 ynode);
    public abstract bool is_leafy(uint64 ynode);
    public abstract bool is_listy(uint64 ynode);
    public abstract bool is_key(uint64 ynode);
    public abstract bool has_keys(uint64 ynode);
    public abstract bool is_presence(uint64 ynode);
    public abstract bool is_mandatory(uint64 ynode);
    public abstract uint64 parent_node(uint64 ynode);
    public abstract uint64 first_child(uint64 ynode);
    public abstract uint64 next_sibling(uint64 ynode);
    public abstract uint64 search_child(uint64 ynode, owned string name, owned string ns);
    public abstract uint64 type(uint64 ynode);
    public abstract uint64 first_value(uint64 ynode);
    public abstract uint64 first_key(uint64 ynode);
    public abstract uint64 first_extension(uint64 ynode);
    public abstract uint64 search_extension(uint64 ynode, owned string module, owned string ext);
    public abstract uint64 module(uint64 ynode);
    public abstract uint64 module_orig(uint64 ynode);
    public abstract bool matches_prefix(uint64 ynode, owned string prefix_string);
    public abstract bool is_mode_path(uint64 ynode);
    public abstract bool is_rpc(uint64 ynode);
    public abstract bool is_rpc_input(uint64 ynode);
  }

  public interface Key: GLib.Object {
    public abstract uint64 list_node(uint64 ykey);
    public abstract uint64 key_node(uint64 ykey);
    public abstract uint64 next_key(uint64 ykey);
  }

  public interface Type: GLib.Object {
    public abstract void location(uint64 ytype, out string str /* Allocated string, owned by the caller! */);
    public abstract unowned string? description(uint64 ytype);
    public abstract unowned string? help_short(uint64 ytype);
    public abstract unowned string? help_full(uint64 ytype);
    public abstract unowned string? prefix(uint64 ytype);
    public abstract unowned string? ns(uint64 ytype);
    /*
    public abstract YangEnums.LeafType leaf_type(uint64 ytype);
    */
    public abstract uint64 first_value(uint64 ytype);
    public abstract uint32 count_matches(uint64 ytype, owned string value_string);
    public abstract uint32 count_partials(uint64 ytype, owned string value_string);
    public abstract uint64 first_extension(uint64 ytype);
  }

  public interface Value: GLib.Object {
    public abstract void location(uint64 yvalue, out string str /* Allocated string, owned by the caller! */);
    public abstract unowned string? name(uint64 yvalue);
    public abstract unowned string? description(uint64 yvalue);
    public abstract unowned string? help_short(uint64 yvalue);
    public abstract unowned string? help_full(uint64 yvalue);
    public abstract unowned string? prefix(uint64 yvalue);
    public abstract unowned string? ns(uint64 yvalue);
    /*
    public abstract YangEnums.LeafType leaf_type(uint64 yvalue);
    */
    public abstract uint64 max_length(uint64 yvalue);
    public abstract uint64 next_value(uint64 yvalue);
    public abstract uint64 first_extension(uint64 yvalue);
    public abstract uint32 parse_value(uint64 yvalue, owned string value_string);
    public abstract uint32 parse_partial(uint64 yvalue, owned string value_string);
    public abstract bool is_keyword(uint64 yvalue);
  }

  public interface Extension: GLib.Object {
    public abstract void location(uint64 yext, out string str /* Allocated string, owned by the caller! */);
    public abstract void location_ext(uint64 yext, out string str /* Allocated string, owned by the caller! */);
    public abstract unowned string? value(uint64 yext);
    public abstract unowned string? description_ext(uint64 yext);
    public abstract unowned string? name(uint64 yext);
    public abstract unowned string? module_ext(uint64 yext);
    public abstract unowned string? prefix(uint64 yext);
    public abstract unowned string? ns(uint64 yext);
    public abstract uint64 next_extension(uint64 yext);
    public abstract uint64 search(uint64 yext, owned string module, owned string ext);
    public abstract bool is_match(uint64 yext, owned string module, owned string ext);
  }
}
