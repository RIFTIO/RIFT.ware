/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import NetworkServiceActions from './launchNetworkServiceActions.js';
import NetworkServiceSource from './launchNetworkServiceSource.js';
import GUID from 'utils/guid.js';
import AppHeaderActions from 'widgets/header/headerActions.js';
import Alt from '../alt';


class LaunchNetworkServiceStore {
    constructor() {
        this.nsd = [];
        this.vnfd = [];
        this.pnfd = [];
        this.name = "";
        this.sla_parameters = [];
        this.selectedNSDid;
        this.selectedCloudAccount = {};
        this.dataCenters = {};
        this.cloudAccounts = [];
        this.isLoading = false;
        this.isStandAlone = false;
        this.hasConfigureNSD = false;
        this['input-parameters'] = [];
        this.displayPlacementGroups = false;
        this.registerAsync(NetworkServiceSource);
        this.exportPublicMethods({
            getMockData: getMockData.bind(this),
            getMockSLA: getMockSLA.bind(this),
            saveNetworkServiceRecord: this.saveNetworkServiceRecord,
            updateSelectedCloudAccount: this.updateSelectedCloudAccount,
            updateSelectedDataCenter: this.updateSelectedDataCenter,
            updateInputParam: this.updateInputParam,
            resetView: this.resetView,
            nameUpdated: this.nameUpdated,
            //nspg
            nsPlacementGroupUpdate: this.nsPlacementGroupUpdate,
            nsHostAggregateUpdate: this.nsHostAggregateUpdate,
            addNsHostAggregate: this.addNsHostAggregate,
            removeNsHostAggregate: this.removeNsHostAggregate,
            //vnfpg
            vnfPlacementGroupUpdate: this.vnfPlacementGroupUpdate,
            vnfHostAggregateUpdate: this.vnfHostAggregateUpdate,
            addVnfHostAggregate: this.addVnfHostAggregate,
            removeVnfHostAggregate: this.removeVnfHostAggregate,
            descriptorSelected: this.descriptorSelected.bind(this)
        });
        this.bindActions(NetworkServiceActions);
        this.descriptorSelected = this.descriptorSelected.bind(this);
        this.nameUpdated = this.nameUpdated.bind(this);
        this.nsdConfiguration = {
            name:'',
            selectedCloudAccount: {},
            dataCenterID: null
        };
    }
    nameUpdated = (name) => {
        this.setState({
            name: name
        })
    }
    updateSelectedCloudAccount = (cloudAccount) => {
        var newState = {
            selectedCloudAccount: cloudAccount
        };
        if (cloudAccount['account-type'] == 'openstack') {
            newState.displayPlacementGroups = true;
        } else {
             newState.displayPlacementGroups = false;
        }
        if (cloudAccount['account-type'] == 'openmano') {
            let datacenter = this.dataCenters[cloudAccount['name']][0];
            // newState.selectedDataCenter = datacenter;
            newState.dataCenterID = datacenter.uuid;
        }
        this.setState(newState);
    }
    updateSelectedDataCenter = (dataCenter) => {
        this.setState({
            dataCenterID: dataCenter
        });
    }
    resetView = () => {
        console.log('reseting state');
        this.setState({
            name: '',
            'input-parameter-xpath': null,
            'ns-placement-groups': null,
            'vnf-placement-groups':null
        })
    }
    descriptorSelected(data) {
        let NSD = data;
        let VNFIDs = [];
        this.resetView();
        let newState = {
            selectedNSDid: NSD.id
        };
        //['input-parameter-xpath']
        if (NSD['input-parameter-xpath']) {
            newState.hasConfigureNSD = true;
            newState['input-parameters'] = NSD['input-parameter-xpath'];
        } else {
            newState.hasConfigureNSD = false;
            newState['input-parameters'] = null;
        }
        if(NSD['ns-placement-groups'] && NSD['ns-placement-groups'].length > 0 ) {
            newState['ns-placement-groups'] = NSD['ns-placement-groups'];
        }
        if(NSD['vnf-placement-groups'] && NSD['vnf-placement-groups'].length > 0 ) {
            newState['vnf-placement-groups'] = NSD['vnf-placement-groups'];
        }
        NSD["constituent-vnfd"].map((v) => {
            VNFIDs.push(v["vnfd-id-ref"]);
        })
        this.getInstance().getVDU(VNFIDs);
        this.setState(newState);
    }

    //Action Handlers
    getCatalogSuccess(catalogs) {
        let self = this;
        let nsd = [];
        let vnfd = [];
        let pnfd = [];
        catalogs.forEach(function(catalog) {
            switch (catalog.type) {
                case "nsd":
                    nsd.push(catalog);
                    try {
                        self.descriptorSelected(catalog.descriptors[0])
                    } catch (e) {}
                    break;
                case "vnfd":
                    vnfd.push(catalog);
                    break;
                case "pnfd":
                    pnfd.push(catalog);
                    break;
            }
        });
        this.setState({
            nsd, vnfd, pnfd
        });
    }
    //refactor getCloudAccountSuccess to use updatedSelectedCA
    getLaunchCloudAccountSuccess(cloudAccounts) {
        let newState = {};
        newState.cloudAccounts = cloudAccounts || [];
        if(cloudAccounts && cloudAccounts.length > 0) {
            newState.selectedCloudAccount = cloudAccounts[0];
            if (cloudAccounts[0]['account-type'] == 'openstack') {
                newState.displayPlacementGroups = true;
            } else {
             newState.displayPlacementGroups = false;
            }
        } else {
            newState.selectedCloudAccount = {};
        }

        this.setState(newState);
    }
    getDataCentersSuccess(dataCenters) {
        let newState = {
            dataCenters: dataCenters
        };
        if (this.selectedCloudAccount['account-type'] == 'openmano') {
            newState.dataCenterID = dataCenters[this.selectedCloudAccount.name][0].uuid
        }
        this.setState(newState)
    }
    getLaunchpadConfigSuccess = (config) => {
        let isStandAlone = ((!config) || config["operational-mode"] == "STANDALONE");
        this.setState({
            isStandAlone: isStandAlone
        });
    }
    getVDUSuccess(VNFD) {
        this.setState({
            sla_parameters: VNFD
        })
    }
    saveNetworkServiceRecord(name, launch) {
        //input-parameter: [{uuid: < some_unique_name>, xpath: <same as you got from nsd>, value: <user_entered_value>}]
        /*
        'input-parameter-xpath':[{
                'xpath': 'someXpath'
            }],
         */
        let nsPg = null;
        let vnfPg = null;
        let guuid = GUID();
        let payload = {
            id: guuid,
            "nsd-ref": this.state.selectedNSDid,
            "name": name,
            "short-name": name,
            "description": "a description for " + guuid,
            "admin-status": launch ? "ENABLED" : "DISABLED"
        }
        if (this.state.isStandAlone) {
            payload["cloud-account"] = this.state.selectedCloudAccount.name;
        }
        if (this.state.selectedCloudAccount['account-type'] == "openmano") {
            payload['om-datacenter'] = this.state.dataCenterID;
        }
        if (this.state.hasConfigureNSD) {
            let ips = this.state['input-parameters'];
            let ipsToSend = ips.filter(function(ip) {
                if (ip.value && ip.value != "") {
                    ip.uuid = GUID();
                    delete ip.name;
                    return true;
                }
                return false;
            });
            if (ipsToSend.length > 0) {
                payload['input-parameter'] = ipsToSend;
            }
        }
        // These placement groups need to be refactored. Too much boilerplate.
        if (this.state.displayPlacementGroups) {
            nsPg = this.state['ns-placement-groups'];
            vnfPg = this.state['vnf-placement-groups'];
            if(nsPg && (nsPg.length > 0)) {
                payload['nsd-placement-group-maps'] = nsPg.map(function(n, i) {
                    if(n['availability-zone'] || n['server-group'] || (n['host-aggregate'].length > 0)) {
                        var obj = {
                            'cloud-type': 'openstack'
                        };
                        if(n['host-aggregate'].length > 0) {
                            obj['host-aggregate'] = n['host-aggregate'].map(function(h, j) {
                                return {
                                    'metadata-key': h.key,
                                    'metadata-value': h.value
                                }
                            })
                        }
                        if(n['availability-zone'] && (n['availability-zone'] != '')) {
                            obj['availability-zone'] = {name: n['availability-zone']};
                        }
                        if(n['server-group'] && (n['server-group'] != '')) {
                            obj['server-group'] = {name: n['server-group']};
                        }
                        obj['placement-group-ref'] = n.name;
                        return obj;
                    }
                }).filter(function(o){
                    if(o) {
                        return true;
                    } else {
                        return false;
                    }
                });
            };
            if(vnfPg && (vnfPg.length > 0)) {
                payload['vnfd-placement-group-maps'] = vnfPg.map(function(n, i) {
                    if(n['availability-zone'] || n['server-group'] || (n['host-aggregate'].length > 0)) {
                        var obj = {
                            'cloud-type': 'openstack'
                        };
                        if(n['host-aggregate'].length > 0) {
                            obj['host-aggregate'] = n['host-aggregate'].map(function(h, j) {
                                return {
                                    'metadata-key': h.key,
                                    'metadata-value': h.value
                                }
                            })
                        }
                        if(n['server-group'] && (n['server-group'] != '')) {
                            obj['server-group'] = {name: n['server-group']};
                        }
                        if(n['availability-zone'] && (n['availability-zone'] != '')) {
                            obj['availability-zone'] = {name: n['availability-zone']};
                        }
                        obj['placement-group-ref'] = n.name;
                        obj['vnfd-id-ref'] = n['vnfd-id-ref'];
                        return obj;
                    }
                }).filter(function(o){
                    if(o) {
                        return true;
                    } else {
                        return false;
                    }
                });
            }
        }
        this.launchNSR({
            'nsr': [payload]
        });
    }
    launchNSRLoading() {
        this.setState({
            isLoading: true
        });
        console.log('is Loading', this)
    }
    launchNSRSuccess(data) {
        console.log('Launching Network Service')
        let tokenizedHash = window.location.hash.split('/');
        this.setState({
            isLoading: false
        });
        return window.location.hash = 'launchpad/' + tokenizedHash[2];
    }
    launchNSRError(error) {
         Alt.actions.global.showError.defer('Something went wrong while trying to instantiate. Check the error logs for more information');
        this.setState({
            isLoading: false
        });
    }
    updateInputParam = (i, value) => {
        let ip = this['input-parameters'];
        ip[i].value = value;
        this.setState({
            'input-parameters': ip
        });
    }
    //nspg
    nsPlacementGroupUpdate = (i, k, value) => {
        let pg = this['ns-placement-groups'];
        pg[i][k] = value;
        this.setState({
            'ns-placement-groups': pg
        })
    }
    nsHostAggregateUpdate = (pgi, hai, k, value) => {
        let pg = this['ns-placement-groups'];
        let ha = pg[pgi]['host-aggregate'][hai];
        ha[k] = value;
        this.setState({
            'ns-placement-groups': pg
        })
    }
    addNsHostAggregate = (pgi) => {
        let pg = this['ns-placement-groups'];
        let ha = pg[pgi]['host-aggregate'];
        ha.push({});
        this.setState({
            'ns-placement-groups': pg
        })
    }
    removeNsHostAggregate = (pgi, hai) => {
        let pg = this['ns-placement-groups'];
        let ha = pg[pgi]['host-aggregate'];
        ha.splice(hai, 1);
        this.setState({
            'ns-placement-groups': pg
        })
    }
    //vnfpg
    vnfPlacementGroupUpdate = (i, k, value) => {
        let pg = this['vnf-placement-groups'];
        pg[i][k] = value;
        this.setState({
            'vnf-placement-groups': pg
        })
    }
    vnfHostAggregateUpdate = (pgi, hai, k, value) => {
        let pg = this['vnf-placement-groups'];
        let ha = pg[pgi]['host-aggregate'][hai];
        ha[k] = value;
        this.setState({
            'vnf-placement-groups': pg
        })
    }
    addVnfHostAggregate = (pgi) => {
        let pg = this['vnf-placement-groups'];
        let ha = pg[pgi]['host-aggregate'];
        ha.push({});
        this.setState({
            'vnf-placement-groups': pg
        })
    }
    removeVnfHostAggregate = (pgi, hai) => {
        let pg = this['vnf-placement-groups'];
        let ha = pg[pgi]['host-aggregate'];
        ha.splice(hai, 1);
        this.setState({
            'vnf-placement-groups': pg
        })
    }
}



function getMockSLA(id) {
    console.log('Getting mock SLA Data for id: ' + id);
    this.setState({
        sla_parameters: slaData
    });
}

function getMockData() {
    console.log('Getting mock Descriptor Data');
    this.setState({
        nsd: data.nsd,
        vnfd: data.vnfd,
        pnfd: data.pnfd
    });
}
// export default Alt.createStore(LaunchNetworkServiceStore);
export default LaunchNetworkServiceStore;
