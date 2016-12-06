
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

