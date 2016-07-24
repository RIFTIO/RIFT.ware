namespace RwMon {
  public interface Monitoring: GLib.Object {
    /**
     * Init routine
     *
     * @param log_ctx - [in] the log context to use
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus init(RwLog.Ctx log_ctx);

    /**
     * nfvi_metrics
     *
     * Returns the NFVI metrics for a particular VM
     *
     * @param account - [in] the account details of the owner of the VM
     * @param vm_id   - [in] the ID of the VM to retrieve the metrics from
     * @param metrics - [out] An NfviMetrics object
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus nfvi_metrics(
      Rwcal.CloudAccount account,
      string vm_id,
      out Rwmon.NfviMetrics metrics);

    /**
     * nfvi_vcpu_metrics
     *
     * Returns the VCPU metrics for a particular VM
     *
     * @param account - [in] the account details of the owner of the VM
     * @param vm_id   - [in] the ID of the VM to retrieve the metrics from
     * @param metrics - [out] An NfviMetrics_Vcpu object
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus nfvi_vcpu_metrics(
      Rwcal.CloudAccount account,
      string vm_id,
      out Rwmon.NfviMetrics_Vcpu metrics);

    /**
     * nfvi_memory_metrics
     *
     * Returns the memory metrics for a particular VM
     *
     * @param account - [in] the account details of the owner of the VM
     * @param vm_id   - [in] the ID of the VM to retrieve the metrics from
     * @param metrics - [out] An NfviMetrics_Memory object
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus nfvi_memory_metrics(
      Rwcal.CloudAccount account,
      string vm_id,
      out Rwmon.NfviMetrics_Memory metrics);

    /**
     * nfvi_storage_metrics
     *
     * Returns the storage metrics for a particular VM
     *
     * @param account - [in] the account details of the owner of the VM
     * @param vm_id   - [in] the ID of the VM to retrieve the metrics from
     * @param metrics - [out] An NfviMetrics_Storage object
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus nfvi_storage_metrics(
      Rwcal.CloudAccount account,
      string vm_id,
      out Rwmon.NfviMetrics_Storage metrics);

    /**
     * nfvi_metrics_available
     *
     * Checks whether ceilometer exists for this account and is 
     * providing NFVI metrics
     *
     * @param account - [in] the account details of the owner of the VM
     * @param present - [out] True if ceilometer exists, False otherwise
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus nfvi_metrics_available(
      Rwcal.CloudAccount account,
      out bool present);

    /**
     * alarm_create
     *
     * @param account - [in] the credentials required to update the alarm.
     * @param vim_id  - [in] the identifier assigned by a VIM to the VDU that
     *                  the alarm is associated with.
     * @param alarm   - [ref] an alarm structure defining the alarm. Note that
     *                  the underlying implmentation will fill in the alarm_id
     *                  attribute, which is required for modifying or deleting
     *                  an alarm.
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus alarm_create(
      Rwcal.CloudAccount account,
      string vim_id,
      ref Rwmon.Alarm alarm);

    /**
     * alarm_update
     *
     * @param account - [in] the credentials required to update the alarm.
     * @param alarm   - [in] the complete alarm structure defining the alarm.
     *                  This means that the alarm structure must contain a
     *                  valid alarm_id, and any attribute of the structure that
     *                  differs from the actual alarm will be updated to match
     *                  the provided data.
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus alarm_update(
      Rwcal.CloudAccount account,
      Rwmon.Alarm alarm);

    /**
     * alarm_delete
     *
     * @param account  - [in] the credentials required to delete the alarm.
     * @param alarm_id - [in] the identifier of the alarm to delete
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus alarm_delete(
      Rwcal.CloudAccount account,
      string alarm_id);

    /**
     * alarm_list
     *
     * @param account  - [in] the credentials required to list the alarms.
     * @param alarms   - [out] a list of alarms
     *
     * @return RwStatus
     */
    public abstract RwTypes.RwStatus alarm_list(
      Rwcal.CloudAccount account,
      out Rwmon.Alarm[] alarms);
  }
}
