
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
