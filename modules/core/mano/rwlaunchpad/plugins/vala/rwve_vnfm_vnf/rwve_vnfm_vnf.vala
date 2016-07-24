namespace RwVeVnfmVnf {

  public interface Vnf: GLib.Object {
    /*
     * Init routine
     */
    public abstract RwTypes.RwStatus init(RwLog.Ctx log_ctx);

    /*
     * Notify the EM of lifecycle event
     */
    public abstract RwTypes.RwStatus get_monitoring_param();
  }
}


