
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
 * @file rwcal.vala
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 *         Austin Cormier (austin.cormier@riftio.com)
 *         Andrew Golovanov (andrw.golovanov@riftio.com)
 * @date 06/12/2014
 * @brief RW.VCS API VALA interface for the RiftWare Virtual Container System
 *
 * @abstract
 * This Vala specification defines the RW.VCS interface API
 * The consumers of this API will be RW module tests for RW.VCS and RW.Main and everything else
 */

// Every plugin belongs to a namespace that matches the names of the .gir and .typelib files

namespace RwvcsApi {

  /**
   * Declare the handle types used by this interface
   * These are backed by plugin specific stuructures in the plugin implementation
   */
  public class ComponentHandle: GLib.Object {
  }

  public class InstanceHandle: GLib.Object {
  }

  public interface Api: GLib.Object {

    /**
     * rwmanifest_pb_to_xml
     *
     * Convert an vcs_manifest protobuf structure into XML
     *
     * @param pbdata
     * Serialized vcs_manifest protobuf structure
     *
     * @param xml[out]
     * String contining and XML representation of the protobuf
     *
     * @return RwTypes.RwStatus
     * RW status for the operation
     */

     public abstract RwTypes.RwStatus
     rwmanifest_pb_to_xml(uint8[] pbdata,
                          out string xml);


 } // public interface Api: GLib.Object

} // namespace RwvcsApi
