
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwpython_util.vala
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/10/2014
 * @brief VALA interface for evaluating python expressions
 *
 * @abstract
 * VALA interface for evaluating python expressions
 */

namespace rwpython_util {
  // This is the interface we will use in the plugin
  public interface Api: GLib.Object {
    public abstract RwTypes.RwStatus
    python_run_string(string python_util);

    public abstract RwTypes.RwStatus
    python_eval_integer(string python_variable, out int result);

    public abstract RwTypes.RwStatus
    python_eval_string(string python_variable, out string result);
  }
}

