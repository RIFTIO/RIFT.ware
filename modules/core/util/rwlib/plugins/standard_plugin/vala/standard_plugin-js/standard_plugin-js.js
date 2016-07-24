const Peas = imports.gi.Peas;
const StandardPlugin = imports.gi.StandardPlugin;
const CommonInterface = imports.gi.RiftCommonInterface;
const GObject = imports.gi.GObject;
const Lang = imports.lang;

function extension_js_plugin() {
  this.read_only = "read-only";
  this.readwrite = "readwrite";
  this.print_done = {};
}

function Module() {
  this._name = ""
  this._alloc_cfg = ""
}

Module.prototype = {
  set_name: function(name) {
    this._name = name
  },

  set_alloc_cfg: function(alloc_cfg) {
    this._alloc_cfg = alloc_cfg
  }
};

function ModuleInstance() {
  this._name = ""
  this._alloc_cfg = ""
}

ModuleInstance.prototype = {
  set_name: function(name) {
   this._name = name
  },

  set_alloc_cfg: function(alloc_cfg) {
    this._alloc_cfg = alloc_cfg
  }
};

extension_js_plugin.prototype = {
  log_function: function(funcname) {
    if (this.print_done[funcname] == null) {
      print ("=============================================");
      print ("This is a call to the javascript plugin " + funcname);
      print ("=============================================");
      this.print_done[funcname] = 1;
    }
  },

  common_activate: function() {
    this.log_function("common_activate");
  },

  common_deactivate: function() {
    this.log_function("common_deactivate");
  },
 
  module_init: function(name, alloc_cfg) {
    this.log_function("module_init");

    a = new Module()
    a.set_name(name)
    a.set_alloc_cfg(alloc_cfg)
    return a;
  },

  module_deinit: function(mod) {
    this.log_function("module_deinit");
    free(mod)    
  },

  add: function(a, b) {
    this.log_function("add");
    return a + b;
  },

  add3: function(a, b) {
    this.log_function("add3" + " " + a + " " + b);
    return a + b;
  }
};

function missing_prerequisite_extension() {}

var extensions = {
  "StandardPluginApi": extension_js_plugin,
  "RiftCommonInterfaceApi": extension_js_plugin,
};
