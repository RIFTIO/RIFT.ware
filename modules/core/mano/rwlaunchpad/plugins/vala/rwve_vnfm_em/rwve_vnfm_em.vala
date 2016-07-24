namespace RwVeVnfmEm {

  public interface ElementManager: GLib.Object {
    /*
     * Init routine
     */
    public abstract RwTypes.RwStatus init(RwLog.Ctx log_ctx);

    /*
     * Notify the EM of lifecycle event
     */
    public abstract RwTypes.RwStatus vnf_lifecycle_event();
  }
}


