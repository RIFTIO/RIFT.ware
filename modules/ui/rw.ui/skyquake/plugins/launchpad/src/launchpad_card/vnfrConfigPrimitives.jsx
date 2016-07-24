
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
 import React from 'react';
 import {Tab, Tabs, TabList, TabPanel} from 'react-tabs';
 import RecordViewStore from '../recordViewer/recordViewStore.js';
 import Button from 'widgets/button/rw.button.js';
 import './vnfrConfigPrimitives.scss';
 export default class VnfrConfigPrimitives extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            vnfrs: []
        };
        this.state.execPrimitive = {};
    }

    handleExecuteClick = (configPrimitiveIndex, vnfrIndex, e) => {
        // let isValid = RecordViewStore.validateInputs({
        //     vnfrConfigPrimitives: this.state.vnfrConfigPrimitives,
        //     vnfrConfigPrimitivesIndex: configPrimitiveIndex
        // });
        let isValid = true;
        if (isValid) {
            RecordViewStore.constructAndTriggerVnfConfigPrimitive({
                vnfrs: this.state.vnfrs,
                configPrimitiveIndex: configPrimitiveIndex,
                vnfrIndex: vnfrIndex
            });    
        }
    }

    handleParamChange = (paramIndex, configPrimitiveIndex, vnfrIndex, e) => {
        let vnfrs = this.state.vnfrs;
        vnfrs[vnfrIndex]["vnf-configuration"]["config-primitive"][configPrimitiveIndex]["parameter"][paramIndex].value = e.target.value
        this.setState({
            vnfrs: vnfrs
        })
    }

    componentWillReceiveProps(props) {
        if (this.state.vnfrs.length == 0) {
            this.setState({
                vnfrs: props.data
            })
        }
    }

    constructConfigPrimitiveTabs = (tabList, tabPanels) => {
        let mandatoryFieldValue = 'true';
        this.state.vnfrs && this.state.vnfrs.map((vnfr, vnfrIndex) => {
            if (vnfr['vnf-configuration'] && vnfr['vnf-configuration']['config-primitive'] && vnfr['vnf-configuration']['config-primitive'].length > 0) {
                vnfr['vnf-configuration']['config-primitive'].map((configPrimitive, configPrimitiveIndex) => {
                    let params = [];
                    if (configPrimitive['parameter'] && configPrimitive['parameter'].length > 0) {
                        configPrimitive['parameter'].map((param, paramIndex) => {
                            let optionalField = '';
                            let displayField = '';
                            let defaultValue = param['default-value'] || '';
                            let isFieldHidden = (param['hidden'] && param['hidden'] == 'true') || false;
                            let isFieldReadOnly = (param['read-only'] && param['read-only'] == 'true') || false;
                            if (param.mandatory == mandatoryFieldValue) {
                                optionalField = <span className="required">*</span>
                            }
                            if (isFieldReadOnly) {
                                displayField = <div className='readonly'>{param.value || defaultValue}</div>
                            } else {
                                displayField = <input required={(param.mandatory == mandatoryFieldValue)} className="" type="text" defaultValue={defaultValue} value={param.value} onChange={this.handleParamChange.bind(this, paramIndex, configPrimitiveIndex, vnfrIndex)} />
                            }
                            params.push(
                                <li key={paramIndex}>
                                    <label className="">
                                        <div>
                                            {param.name} {optionalField}
                                        </div>
                                        {displayField}
                                    </label>
                                </li>
                            );
                        });
                    }
                    tabList.push(
                        <Tab key={configPrimitiveIndex}>{configPrimitive.name}</Tab>
                    );

                    tabPanels.push(
                        <TabPanel key={configPrimitiveIndex + '-panel'}>
                            <div>
                                <ul className="parameterGroup">
                                    {params}
                                </ul>
                            </div>
                            <div style={{marginTop: '1rem', marginLeft: '2rem'}}>
                                * required
                            </div>
                            <button className="dark" role="button" onClick={this.handleExecuteClick.bind(this, configPrimitiveIndex, vnfrIndex)}>{configPrimitive.name}</button>
                        </TabPanel>
                    )
                });
            }
        });
    }

    render() {

        let tabList = [];
        let tabPanels = [];
        let isConfiguring = (this.props.data['config-status'] && this.props.data['config-status'] != 'configured') || false;
        let displayConfigStatus = isConfiguring ? '(Disabled - Configuring)': '';

        this.constructConfigPrimitiveTabs(tabList, tabPanels);

        return (
            <div className="vnfrConfigPrimitives">
                <div className="launchpadCard_title">
                  CONFIG-PRIMITIVES {displayConfigStatus}
                </div>
                <div className={isConfiguring ? 'configuring': ''}>
                    <Tabs onSelect={this.handleSelect}>
                        <TabList>
                            {tabList}
                        </TabList>
                        {tabPanels}
                    </Tabs>
                </div>
            </div>

        );
    }
}