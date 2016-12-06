
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
 * @file rwcli_plugin.vala
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 03/10/2014
 * @brief Default vala interface for CLI printing
 * 
 * @abstract
 * This vala specification defines the interface API for adding custom CLI print routines
 */

using RwYang;

namespace rwcli_plugin {
  public interface Print: GLib.Object {
    /**
     * Default print routine
     */
    public abstract void
    default_print(string xml_string);

    /**
     * Custom print
     */
    public abstract void
    pretty_print(string xml_string, string print_routine);

    /**
     * Print using the CLI Yang model
     */
    public abstract void
    print_using_model(Model model, 
                      string xml_string,
                      string print_routine,
                      string out_file);
  }
}
