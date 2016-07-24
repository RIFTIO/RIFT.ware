import pytest

from gi.repository import NsrYang, RwNsrYang, RwVnfrYang, NsdYang, RwNsdYang
import rift.auto.session

@pytest.fixture(scope='module')
def proxy(request, launchpad_session):
    return launchpad_session.proxy


ScalingGroupInstance = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup_Instance
ScalingGroup = NsrYang.YangData_Nsr_NsInstanceConfig_Nsr_ScalingGroup

INSTANCE_ID = 1


@pytest.mark.depends('nsr')
@pytest.mark.incremental
class TestScaling:
    def wait_for_nsr_state(self, proxy, state):
        """Wait till the NSR reaches a desired state.

        Args:
            proxy (Callable): Proxy for launchpad session.
            state (str): Expected state
        """
        nsr_opdata = proxy(RwNsrYang).get('/ns-instance-opdata')
        nsr = nsr_opdata.nsr[0]
        xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/operational-status".format(nsr.ns_instance_config_ref)
        proxy(RwNsrYang).wait_for(xpath, state, timeout=240)

    def verify_scaling_group(self, proxy, group_name, expected_records_count, scale_out=True):
        """
        Args:
            proxy (Callable): LP session
            group_name (str): Group name which is being scaled up.
            scale_out (bool, optional): To identify scale-out/scale-in mode.

        Asserts:
            1. Additional records are added to the opdata
            2. Status of the scaling group
            3. New vnfr record has been created.
        """
        nsr_opdata = proxy(RwNsrYang).get('/ns-instance-opdata')
        nsr_id = nsr_opdata.nsr[0].ns_instance_config_ref

        xpath = ('/ns-instance-opdata/nsr[ns-instance-config-ref="{}"]'
                 '/scaling-group-record[scaling-group-name-ref="{}"]').format(
                        nsr_id, group_name)

        scaling_record = proxy(NsrYang).get(xpath)

        assert len(scaling_record.instance) == expected_records_count

        for instance in scaling_record.instance:
            assert instance.op_status == 'running'

            for vnfr in instance.vnfrs:
                vnfr_record = proxy(RwVnfrYang).get(
                        "/vnfr-catalog/vnfr[id='{}']".format(vnfr))
                assert vnfr_record is not None

    def verify_scale_up(self, proxy, group_name, expected):
        """Verifies the scaling up steps for the group
        NSR moves from running -> scaling-up -> running

        Args:
            proxy (callable): LP proxy
            group_name (str): Name of the group to verify.
        """
        self.wait_for_nsr_state(proxy, "scaling-out")
        self.wait_for_nsr_state(proxy, "running")
        self.verify_scaling_group(proxy, group_name, expected)

    def verify_scale_in(self, proxy, group_name, expected):
        """Verifies the scaling in streps for the group.
        NSR moves from running -> scaling-down -> running

        Args:
            proxy (callable): LP proxy
            group_name (str): group name.
        """
        self.wait_for_nsr_state(proxy, "scaling-in")
        self.wait_for_nsr_state(proxy, "running")
        self.verify_scaling_group(proxy, group_name, expected, scale_out=False)

    def test_wait_for_nsr_configured(self, proxy):
        """Wait till the NSR state moves to configured before starting scaling
        tests.
        """
        nsr_opdata = proxy(RwNsrYang).get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr

        assert len(nsrs) == 1
        current_nsr = nsrs[0]

        xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/config-status".format(current_nsr.ns_instance_config_ref)
        proxy(RwNsrYang).wait_for(xpath, "configured", timeout=240)


    def test_min_max_scaling(self, proxy):
        nsr_opdata = proxy(RwNsrYang).get('/ns-instance-opdata')
        nsrs = nsr_opdata.nsr
        nsd_id = nsrs[0].nsd_ref
        nsr_id = nsrs[0].ns_instance_config_ref

        # group_name = "http_client_group"

        xpath = "/ns-instance-opdata/nsr[ns-instance-config-ref='{}']/scaling-group-record".format(nsr_id)
        scaling_records = proxy(RwNsrYang).get(xpath, list_obj=True)

        for scaling_record in scaling_records.scaling_group_record:
            group_name = scaling_record.scaling_group_name_ref
            xpath = "/nsd-catalog/nsd[id='{}']/scaling-group-descriptor[name='{}']".format(
                    nsd_id, group_name)
            scaling_group_desc = proxy(NsdYang).get(xpath)

            # Add + 1 to go beyond the threshold
            for instance_id in range(1, scaling_group_desc.max_instance_count + 1):
                xpath = '/ns-instance-config/nsr[id="{}"]/scaling-group[scaling-group-name-ref="{}"]'.format(
                            nsr_id, 
                            group_name)

                instance = ScalingGroupInstance.from_dict({"id": instance_id})
                scaling_group = proxy(NsrYang).get(xpath)

                if scaling_group is None:
                    scaling_group = ScalingGroup.from_dict({
                        'scaling_group_name_ref': group_name,
                        })

                scaling_group.instance.append(instance)

                try:
                    proxy(NsrYang).merge_config(xpath, scaling_group)
                    self.verify_scale_up(proxy, group_name, instance_id + 1)
                except rift.auto.session.ProxyRequestError:
                    assert instance_id == scaling_group_desc.max_instance_count

            for instance_id in range(1, scaling_group_desc.max_instance_count):
                xpath = ('/ns-instance-config/nsr[id="{}"]/scaling-group'
                         '[scaling-group-name-ref="{}"]/'
                         'instance[id="{}"]').format(
                         nsr_id, group_name, instance_id)
                proxy(NsrYang).delete_config(xpath)
                self.verify_scale_in(proxy, group_name, instance_id)







