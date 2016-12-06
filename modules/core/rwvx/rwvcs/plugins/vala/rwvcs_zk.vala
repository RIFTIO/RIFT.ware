namespace RwVcsZk {

  public enum KazooClientStateEnum {
    LOST,
    SUSPENDED,
    CONNECTED
  }

  public class ServerPortDetail : GLib.Object {
    public string zk_server_addr;
    public int zk_client_port;
    public int zk_server_id;
    public int zk_server_pid;
    public bool zk_server_disable;
    public bool zk_in_config;
    public bool zk_server_start;
    public bool zk_client_connect;
  }

  public static delegate void rwvcs_zk_callback(void * rwvcs_zk, void * user_data, int length);
  public class Closure: GLib.Object {
    public void * m_rwvcs_zk;
    public void * m_user_data;
    [CCode (array_length = false, array_null_terminated = true)]
    public string [] m_data;
    public rwvcs_zk_callback m_callback;

    public void store_data([CCode (array_length = false, array_null_terminated = true)]
                           string [] data_in) {
      int idx = 0;
      m_data = new string[data_in.length];
      while (idx < data_in.length) {
        m_data[idx] = data_in[idx];
        idx = idx + 1;
      }
    }
    public void callback() {
      void * [] ret_ptrs = {};
      ret_ptrs += m_user_data;
      int idx = 0;
      while (idx < m_data.length) {
        ret_ptrs += m_data[idx];
        idx = idx + 1;
      }
      m_callback(m_rwvcs_zk, ret_ptrs, ret_ptrs.length);
    }
  }

  public interface Zookeeper: GLib.Object {
    /* These functions are just wrappers on what is exposed by the python
     * module rift.rwzk.  See there for actual documentation.
     */

    public abstract RwTypes.RwStatus create_server_config(
      int id,
      int client_port,
      bool unique_ports,
      [CCode (array_length = false, array_null_terminated = true)]
      ServerPortDetail [] server_details,
      string rift_var_root);

    public abstract int server_start(int id);

    public abstract RwTypes.RwStatus kazoo_init(
      [CCode (array_length = false, array_null_terminated = true)]
      ServerPortDetail [] server_details);

    public abstract RwTypes.RwStatus zake_init();

    public abstract RwTypes.RwStatus kazoo_start();

    public abstract RwTypes.RwStatus kazoo_stop();

    public abstract RwTypes.RwStatus lock(string path, float timeout);

    public abstract RwTypes.RwStatus unlock(string path);

    public abstract bool locked(string path);

    public abstract RwTypes.RwStatus create(string path,
                                            Closure closure);

    public abstract bool exists(string path);

    public abstract RwVcsZk.KazooClientStateEnum get_client_state();

    public abstract RwTypes.RwStatus get(string path, out string data,
                                         Closure closure);

    public abstract RwTypes.RwStatus set(string path, string data,
                                         Closure closure);

    public abstract RwTypes.RwStatus children(
      string path,
      [CCode (array_length = false, array_null_terminated = true)]
      out string [] children,
      Closure closure);

    public abstract RwTypes.RwStatus rm(string path,
                                        Closure closure);

    public abstract RwTypes.RwStatus register_watcher(string path,
                                                      Closure closure);

    public abstract RwTypes.RwStatus unregister_watcher(string path,
                                                        Closure closure);
  }
}


