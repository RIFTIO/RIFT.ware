const Peas = imports.gi.Peas;
const BasicPlugin = imports.gi.BasicPlugin;

function extension_js_plugin() {
  this.read_only = "read-only";
  this.readwrite = "readwrite";
  this.print_done = 0;
}

extension_js_plugin.prototype = {
  activate: function() {
  },
  deactivate: function() {
  },

  get_plugin_info: function() {
    return this.plugin_info;
  },
  get_settings: function() {
    return this.plugin_info.get_settings(null);
  },

  call_with_return: function() {
    return "Hello, World!";
  },
  call_no_args: function() {
  },
  call_single_arg: function() {
    return true;
  },
  call_multi_args: function(in_, inout) {
    return [ inout, in_ ];
  },
  add: function(a, b) {
    if (this.print_done == 0) {
	print("=======================================");
	print("This is a call to the Javascript plugin");
	print("=======================================");
        this.print_done = 1;
    }
    return a + b;
  },
  add3: function(a, b) {
    if (this.print_done == 0) {
	print("=======================================");
	print("This is a call to the Javascript plugin add3()");
        print(a);
        print(b);
        print(c);
	print("=======================================");
        this.print_done = 1;
    }
    return a+b;
  }
};

function missing_prerequisite_extension() {}

var extensions = {
  "PeasActivatable": extension_js_plugin,
  "BasicPluginApi": extension_js_plugin,
};
