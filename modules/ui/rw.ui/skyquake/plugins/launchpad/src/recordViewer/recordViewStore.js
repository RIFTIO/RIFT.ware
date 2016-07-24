/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import RecordViewActions from './recordViewActions.js';
import RecordViewSource from './recordViewSource.js';
// import source
// import AppHeaderActions from 'widgets/header/headerActions.js';
import Alt from '../alt';
import _ from 'underscore';

class RecordViewStore {
    constructor() {
        this.isLoading = true;
        this.cardLoading = true;
        this.detailLoading = true;
        //Reference to current socket
        this.socket = null;
        //Reference to current record type
        //"vnfr", "nsr", "default"
        this.recordType = "default";
        //Reference to current record ID
        //uuid or null
        this.recordID = null;
        //Record data
        //object or null
        this.recordData = null;
        this.nav = [];
        this.vnfrs = {};
        this.configPrimitives = [];
        this.bindActions(RecordViewActions);
        this.exportPublicMethods({
            constructAndTriggerVnfConfigPrimitive: this.constructAndTriggerVnfConfigPrimitive,
            constructAndTriggerNsConfigPrimitive: this.constructAndTriggerNsConfigPrimitive,
            triggerCreateScalingGroupInstance: this.triggerCreateScalingGroupInstance,
            triggerDeleteScalingGroupInstance: this.triggerDeleteScalingGroupInstance,
            validateInputs: this.validateInputs
        })
        this.exportAsync(RecordViewSource);
    }
    handleLogout = () => {

    }
    handleCloseSocket = () => {
        if (this.socket) {
            this.socket.close();
        }
    }
    loadRecord(record) {
        this.setState({
            cardLoading: true,
            recordID: record.id
        });
    }
    getNSRSocketLoading() {
        this.setState({
            cardLoading: true
        })
    }
    getVNFRSocketLoading() {
        this.setState({
            cardLoading: true
        })
    }
    getVNFRSocketSuccess(connection) {
        // debugger;
        connectionManager.call(this, 'vnfr', connection);
    }
    getNSRSocketSuccess(connection) {
        connectionManager.call(this, 'nsr', connection);
    }
    getRawLoading() {
        this.setState({
            detailLoading: true
        })
    }
    getRawSuccess(data) {
        this.setState({
            rawData: data,
            detailLoading: false
        })
    }
    getNSRSuccess(data) {

    }
    constructAndTriggerVnfConfigPrimitive(data) {
        let vnfrs = data.vnfrs;
        let vnfrIndex = data.vnfrIndex;
        let configPrimitiveIndex = data.configPrimitiveIndex;
        let payload = {};

        let configPrimitive = vnfrs[vnfrIndex]['vnf-configuration']['config-primitive'][configPrimitiveIndex];

        payload['name'] = '';
        payload['nsr_id_ref'] = vnfrs[vnfrIndex]['nsr-id-ref'];
        payload['vnf-list'] = [];

        let parameters = [];
        configPrimitive['parameter'].map((parameter) => {
            parameters.push({
                name: parameter['name'],
                value: parameter['value']
            });
        });

        let vnfPrimitive = [];
        vnfPrimitive[0] = {
            name: configPrimitive['name'],
            index: configPrimitiveIndex,
            parameter: parameters
        }

        payload['vnf-list'].push({
            'member_vnf_index_ref': vnfrs[vnfrIndex]['member-vnf-index-ref'],
            'vnfr-id-ref': vnfrs[vnfrIndex]['id'],
            'vnf-primitive': vnfPrimitive
        })

        this.execNsConfigPrimitive(payload);
    }
    constructAndTriggerNsConfigPrimitive(data) {
        let nsConfigPrimitiveIndexToExecute = data.nsConfigPrimitiveIndex;
        let nsConfigPrimitives = data.nsConfigPrimitives;
        let nsConfigPrimitive = data.nsConfigPrimitives[nsConfigPrimitiveIndexToExecute];

        let payload = {
            name: nsConfigPrimitive['name'],
            nsr_id_ref: nsConfigPrimitive['nsr_id_ref'],
            'vnf-list': [],
            'parameter': [],
            'parameter-group': [],
        };

        let vnfList = [];
        nsConfigPrimitive['vnf-primitive-group'].map((vnf) => {

            let vnfPrimitiveList = []
            vnf['inputs'].map((vnfPrimitive) => {

                let parameterList = [];

                const filterAndAddByValue = (paramObj) => {
                    if (paramObj['value'] != undefined) {
                        parameterList.push({
                            name: paramObj.name,
                            value: paramObj.value
                        });
                    }
                    return paramObj['value'] != undefined;
                }

                vnfPrimitive['parameter'].filter(filterAndAddByValue);

                if (parameterList.length > 0) {
                    vnfPrimitiveList.push({
                        name: vnfPrimitive['name'],
                        index: vnfPrimitive['index'],
                        parameter: parameterList
                    });
                }
            });

            vnfList.push({
                'member_vnf_index_ref': vnf['member-vnf-index-ref'],
                'vnfr-id-ref': vnf['vnfr-id-ref'],
                'vnf-primitive': vnfPrimitiveList
            });
        });

        payload['vnf-list'] = vnfList;

        let nsConfigPrimitiveParameterGroupParameters = [];
        nsConfigPrimitive['parameter-group'] && nsConfigPrimitive['parameter-group'].map((nsConfigPrimitiveParameterGroup) => {
            let nsConfigPrimitiveParameters = [];
            nsConfigPrimitiveParameterGroup['parameter'] && nsConfigPrimitiveParameterGroup['parameter'].map((nsConfigPrimitiveParameterGroupParameter) => {
                if (nsConfigPrimitiveParameterGroupParameter['value'] != undefined) {
                    nsConfigPrimitiveParameters.push({
                        'name': nsConfigPrimitiveParameterGroupParameter.name,
                        'value': nsConfigPrimitiveParameterGroupParameter.value
                    });
                }
            });
            nsConfigPrimitiveParameterGroupParameters.push({
                'name': nsConfigPrimitiveParameterGroup.name,
                'parameter': nsConfigPrimitiveParameters
            });
        });

        payload['parameter-group'] = nsConfigPrimitiveParameterGroupParameters;

        let nsConfigPrimitiveParameters = [];
        nsConfigPrimitive['parameter'] ? nsConfigPrimitive['parameter'].map((nsConfigPrimitiveParameter) => {
            if (nsConfigPrimitiveParameter['value'] != undefined) {
                nsConfigPrimitiveParameters.push({
                    'name': nsConfigPrimitiveParameter.name,
                    'value': nsConfigPrimitiveParameter.value
                });
            }
        }):null;

        payload['parameter'] = nsConfigPrimitiveParameters;

        this.execNsConfigPrimitive(payload);
    }
    execNsConfigPrimitiveSuccess(data) {}
    createScalingGroupInstanceSuccess(data) {}
    deleteScalingGroupInstanceSuccess(data) {}
    triggerCreateScalingGroupInstance(params) {
        this.createScalingGroupInstance(params);
    }
    triggerDeleteScalingGroupInstance(params) {
        this.deleteScalingGroupInstance(params);
    }
    validateInputs(data) {
        let nsConfigPrimitiveIndexToExecute = data.nsConfigPrimitiveIndex;
        let nsConfigPrimitives = data.nsConfigPrimitives;
        let nsConfigPrimitive = data.nsConfigPrimitives[nsConfigPrimitiveIndexToExecute];
        let isValid = true;
        //Check parameter groups for required fields
        nsConfigPrimitive['parameter-group'] && nsConfigPrimitive['parameter-group'].map((parameterGroup, parameterGroupIndex) => {
            let isMandatory = parameterGroup.mandatory != 'false';
            let optionalChecked = parameterGroup.optionalChecked;
            let isActiveOptional = (optionalChecked && !isMandatory);
            if (isActiveOptional || isMandatory) {
                parameterGroup['parameter'] && parameterGroup['parameter'].map((parameter, parameterIndex) => {
                    let msg = 'Parameter Group: ' + parameterGroup.name + ' is not valid';
                    validateParameter(parameter, msg);
                });
            }
        });

        //Check top level parameters for required fields
        nsConfigPrimitive['parameter'] && nsConfigPrimitive['parameter'].map((parameter, parameterIndex) => {
            // let isMandatory = parameter.mandatory != 'false';
            // if (isMandatory) {
                let msg = 'Parameter: ' + parameter.name + ' is not valid'
                validateParameter(parameter, msg)
            // }

        });

        nsConfigPrimitive['vnf-primitive-group'] && nsConfigPrimitive['vnf-primitive-group'].map((vnfPrimitiveGroup, vnfPrimitiveGroupIndex) => {
            vnfPrimitiveGroup['inputs'] && vnfPrimitiveGroup['inputs'].map((input, inputIndex) => {
                input['parameter'] && input['parameter'].map((param) => {
                    let msg = 'Parameter: ' + param.name + ' is not valid';
                    validateParameter(param, msg)
                })
            })
        });


        function validateParameter(parameter, msg) {
            if ((parameter['hidden'] == 'true') || (parameter['read-only'] == 'true')) {
                if (!parameter['default-value']) {
                    var errorMsg = 'Your descriptor has hidden or read-only parameters with no default-values. Please rectify this.';
                    console.log(errorMsg);
                    AppHeaderActions.validateError(errorMsg);
                    isValid = false;
                    return false;
                } else {
                    parameter.value = parameter['default-value'];
                    return true;
                }
            }

            if (parameter.mandatory == "true") {
                if (!parameter.value) {
                    if (!parameter['default-value']) {
                        AppHeaderActions.validateError(msg);
                        isValid = false;
                        return false;
                    } else {
                        parameter.value = parameter['default-value'];
                        return true;
                    }
                }
            };
            return true;
        };
        return isValid;
    }

}


function connectionManager(type, connection) {
    let self = this;
    if (!connection) {
        console.warn('There was an issue connecting to the ' + type + ' socket');
        return;
    }
    if (self.socket) {
        self.socket.close();
        self.socket = null;
    }
    self.setState({
        socket: connection
    });
    connection.onmessage = function(data) {

        let recordData = JSON.parse(data.data);

        let nsrsData = recordData['nsrs'] || null;

        let navigatorState = {};

        if (nsrsData) {
            let nav = [];
            let nsrs = nsrsData[0];
            nav.push({
                name: nsrs.name,
                id: nsrs.id,
                nsd_name: nsrs.nsd_name,
                type: 'nsr'
            });

            // Scaled VNFRs
            let scaledVnfrs = [];

            nsrs['scaling-group-record'] && nsrs['scaling-group-record'].map((sgr, sgrIndex) => {
                sgr['instance'] && sgr['instance'].map((sgInstance, sgInstanceIndex) => {
                    let scaledVnfNav = {
                        type: 'sgr',
                        scalingGroupName: sgr['scaling-group-name-ref'],
                        scalingGroupInstanceId: sgInstance['instance-id'],
                        vnfr: []
                    }

                    sgInstance['vnfrs'] && sgInstance['vnfrs'].map((vnfr, vnfrIndex) => {
                        scaledVnfrs.push(vnfr);
                        scaledVnfNav.vnfr.push({
                            name: _.findWhere(nsrs.vnfrs, {id: vnfr})['short-name'],
                            id: vnfr,
                            type: 'vnfr'
                        });
                    });
                    nav.push(scaledVnfNav);
                });
            });

            // Non-scaled VNFRs
            nsrs.vnfrs.map(function(vnfr, vnfrIndex) {
                if (_.indexOf(scaledVnfrs, vnfr.id) == -1) {
                    nav.push({
                        name: vnfr["short-name"],
                        id: vnfr.id,
                        type: 'vnfr'
                    });
                }
            });
            navigatorState = {
                nav: nav,
                recordID: nsrs.id,
                isLoading: false,
            };
        }

        navigatorState = _.extend(navigatorState, {
            recordData: recordData,
            recordType: type,
            cardLoading: false,
            // ,isLoading: false
        });

        self.setState(navigatorState);
    };
}

export default Alt.createStore(RecordViewStore);
