namespace RwShell {

  public interface Perftools: GLib.Object {
    /* These functions are just wrappers on what is exposed by the python
     * module rwshell.perftools.  See there for actual documentation.
     */

    public abstract RwTypes.RwStatus start_perf(
      [CCode (array_length = false, array_null_terminated = true)]
      string [] pids,
      uint frequency,
      string perf_data_fname,
      [CCode (array_length = false, array_null_terminated = true)]
      out string command,
      out int newpid);

    public abstract RwTypes.RwStatus stop_perf(
      int newpid,
      [CCode (array_length = false, array_null_terminated = true)]
      out string command);

    public abstract RwTypes.RwStatus report_perf(
      double percent_limit,
      string perf_data_fname,
      [CCode (array_length = false, array_null_terminated = true)]
      out string command,
      [CCode (array_length = false, array_null_terminated = true)]
      out string report);
  }

  public interface Crashtools: GLib.Object {
    /* These functions are just wrappers on what is exposed by the python
     * module rwshell.crashtools.  See there for actual documentation.
     */

    public abstract RwTypes.RwStatus report_crash(
      string vm_name,
      [CCode (array_length = false, array_null_terminated = true)]
      out string [] crashes);
  }
}
