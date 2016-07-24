namespace RwOsMaNfvo {

  public interface Orchestrator: GLib.Object {
    /*
     * Init routine
     */
    public abstract RwTypes.RwStatus init(RwLog.Ctx log_ctx);

    /*
     * Notify the EM of lifecycle event
     */
    public abstract RwTypes.RwStatus ns_lifecycle_event();
  }
}


