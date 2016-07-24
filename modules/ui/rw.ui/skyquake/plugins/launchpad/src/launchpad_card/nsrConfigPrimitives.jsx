
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import {Tab, Tabs, TabList, TabPanel} from 'react-tabs';
import RecordViewStore from '../recordViewer/recordViewStore.js';
import Button from 'widgets/button/rw.button.js';
import './nsConfigPrimitives.scss';
export default class NsrConfigPrimitives extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
        this.state.vnfrMap = null;
        this.state.nsConfigPrimitives = null;
    }

    componentWillReceiveProps(props) {
        let vnfrs = props.data['vnfrs'];
        let vnfrMap = {};
        let nsConfigPrimitives = [];
        let vnfList = [];
        if (vnfrs &&  !this.state.nsConfigPrimitives) {
            vnfrs.map((vnfr) => {
                let vnfrConfigPrimitive = {};
                vnfrConfigPrimitive.name = vnfr['short-name'];
                vnfrConfigPrimitive['vnfr-id-ref'] = vnfr['id'];
                //input references
                let configPrimitives = vnfr['vnf-configuration']['config-primitive'];
                //input references by key
                let vnfrConfigPrimitives = {}

                if (configPrimitives) {
                    let vnfPrimitiveList = [];
                    configPrimitives.map((configPrimitive) => {
                        vnfrConfigPrimitives[configPrimitive.name] = configPrimitive;
                        vnfPrimitiveList.push({
                            name: configPrimitive.name,
                            parameter: configPrimitive.parameter
                        })
                    });
                    vnfrConfigPrimitive['config-primitives'] = vnfrConfigPrimitives;
                    vnfrMap[vnfr['member-vnf-index-ref']] = vnfrConfigPrimitive;
                }
            });
            //nsr config-primitives
            props.data['config-primitive'] && props.data['config-primitive'].map((nsConfigPrimitive, nsConfigPrimitiveIndex) => {
                 //Add optional field to group. Necessary for driving state.
                let optionalizedNsConfigPrimitiveGroup = nsConfigPrimitive['parameter-group'] && nsConfigPrimitive['parameter-group'].map((parameterGroup)=>{
                    if(parameterGroup && parameterGroup.mandatory != "true") {
                        parameterGroup.optionalChecked = true;
                    };
                    return parameterGroup;
                });
                let nscp = {
                    name: nsConfigPrimitive.name,
                    'nsr_id_ref': props.data.id,
                    'parameter': nsConfigPrimitive.parameter,
                    'parameter-group': optionalizedNsConfigPrimitiveGroup,
                    'vnf-primitive-group':[]
                }

                nsConfigPrimitive['vnf-primitive-group'] && nsConfigPrimitive['vnf-primitive-group'].map((vnfPrimitiveGroup, vnfPrimitiveGroupIndex) => {
                    let vnfMap = vnfrMap[vnfPrimitiveGroup['member-vnf-index-ref']];
                    let vnfGroup = {};
                    vnfGroup.name = vnfMap.name;
                    vnfGroup['member-vnf-index-ref'] = vnfPrimitiveGroup['member-vnf-index-ref'];
                    vnfGroup['vnfr-id-ref'] = vnfMap['vnfr-id-ref'];
                    vnfGroup.inputs = [];
                    vnfPrimitiveGroup.primitive.map((primitive, primitiveIndex) => {
                        console.log(primitive);
                        primitive.index = primitiveIndex;
                        primitive.parameter = [];
                        vnfMap['config-primitives'][primitive.name].parameter.map(function(p) {
                            p.index = primitiveIndex;
                            let paramCopy = p;
                            paramCopy.value = undefined;
                            primitive.parameter.push(paramCopy);
                        });
                        vnfGroup.inputs.push(primitive);
                    });
                    nscp['vnf-primitive-group'].push(vnfGroup);
                });
                nsConfigPrimitives.push(nscp);
            });
            this.setState({
                vnfrMap: vnfrMap,
                data: props.data,
                nsConfigPrimitives: nsConfigPrimitives
            });
        }
    }

    handleParamChange = (parameterIndex, vnfPrimitiveIndex, vnfListIndex, nsConfigPrimitiveIndex, event) => {
        let nsConfigPrimitives = this.state.nsConfigPrimitives;
        nsConfigPrimitives[nsConfigPrimitiveIndex]['vnf-primitive-group'][vnfListIndex]['inputs'][vnfPrimitiveIndex]['parameter'][parameterIndex]['value'] = event.target.value;
        this.setState({
            nsConfigPrimitives: nsConfigPrimitives
        });
    }

    handleNsParamChange = (parameterIndex, nsConfigPrimitiveIndex, event) => {
        let nsConfigPrimitives = this.state.nsConfigPrimitives;
        nsConfigPrimitives[nsConfigPrimitiveIndex]['parameter'][parameterIndex]['value'] = event.target.value;
        this.setState({
            nsConfigPrimitives: nsConfigPrimitives
        });
    }

    handleNsParamGroupParamChange = (parameterIndex, parameterGroupIndex, nsConfigPrimitiveIndex, event) => {
        let nsConfigPrimitives = this.state.nsConfigPrimitives;
        nsConfigPrimitives[nsConfigPrimitiveIndex]['parameter-group'][parameterGroupIndex]['parameter'][parameterIndex]['value'] = event.target.value;
        this.setState({
            nsConfigPrimitives: nsConfigPrimitives
        });
    }

    handleExecuteClick = (nsConfigPrimitiveIndex, event) => {
        var isValid = RecordViewStore.validateInputs({
            nsConfigPrimitives: this.state.nsConfigPrimitives,
            nsConfigPrimitiveIndex: nsConfigPrimitiveIndex
        });
        if(isValid) {
            RecordViewStore.constructAndTriggerNsConfigPrimitive({
                nsConfigPrimitives: this.state.nsConfigPrimitives,
                nsConfigPrimitiveIndex: nsConfigPrimitiveIndex
             });
            //Need a better way to reset
            //Reset disabled per TEF request
            // this.setState({
            //     nsConfigPrimitives: null
            // })
        }
    }
    handleOptionalCheck = (parameterGroupIndex, nsConfigPrimitiveIndex, event) => {
        let nsConfigPrimitives = this.state.nsConfigPrimitives;
        let optionalChecked = nsConfigPrimitives[nsConfigPrimitiveIndex]['parameter-group'][parameterGroupIndex].optionalChecked;
        nsConfigPrimitives[nsConfigPrimitiveIndex]['parameter-group'][parameterGroupIndex].optionalChecked = !optionalChecked;
        console.log(nsConfigPrimitives)
        this.setState({
            nsConfigPrimitives: nsConfigPrimitives
        });
    }
    constructConfigPrimitiveTabs = (tabList, tabPanels) => {
        let self = this;
        let defaultFromRpc = '';
        //coded here for dev purposes
        let mandatoryFieldValue = 'true';
        if (self.state.vnfrMap) {
            this.state.nsConfigPrimitives && this.state.nsConfigPrimitives.map((nsConfigPrimitive, nsConfigPrimitiveIndex) => {
                tabList.push(
                    <Tab key={nsConfigPrimitiveIndex}>{nsConfigPrimitive.name}</Tab>
                );
                tabPanels.push(
                    (
                        <TabPanel key={nsConfigPrimitiveIndex + '-panel'}>
                            {nsConfigPrimitive['parameter'] && nsConfigPrimitive['parameter'].map((parameter, parameterIndex) => {
                                let optionalField = '';
                                let displayField = '';
								let defaultValue = parameter['default-value'] || '';
                                let isFieldHidden = (parameter['hidden'] && parameter['hidden'] == 'true') || false;
                                let isFieldReadOnly = (parameter['read-only'] && parameter['read-only'] == 'true') || false;
                                if (parameter.mandatory == mandatoryFieldValue) {
                                    optionalField = <span className="required">*</span>
                                }
                                if (isFieldReadOnly) {
                                    displayField = <div className='readonly'>{parameter.value || defaultValue}</div>
                                } else {
                                    displayField = <input required={(parameter.mandatory == mandatoryFieldValue)} className="" type="text" defaultValue={defaultValue} value={parameter.value} onChange={this.handleNsParamChange.bind(this, parameterIndex, nsConfigPrimitiveIndex)} />
                                }
                                return (
                                    <div key={parameterIndex} className="nsConfigPrimitiveParameters" style={{display: isFieldHidden ? 'none':'inherit'}}>
                                        <ul>
                                        {
                                            <li key={parameterIndex} className="">
                                                <label data-required={(parameter.mandatory == mandatoryFieldValue)}>
                                                    {parameter.name}
                                                    {displayField}
                                                </label>

                                            </li>
                                        }
                                        </ul>
                                    </div>
                                )
                            })}
                            {nsConfigPrimitive['parameter-group'] && nsConfigPrimitive['parameter-group'].map((parameterGroup, parameterGroupIndex) => {
                                let optionalField = '';
                                let overlayGroup = null;
                                let isOptionalGroup = parameterGroup.mandatory != mandatoryFieldValue;
                                let optionalChecked = parameterGroup.optionalChecked;
                                let inputIsDiabled = (!optionalChecked && isOptionalGroup);
                                if (isOptionalGroup) {
                                    optionalField = <input type="checkbox" name="" checked={optionalChecked} onChange={self.handleOptionalCheck.bind(null, parameterGroupIndex, nsConfigPrimitiveIndex)} />;
                                    // overlayGroup = <div className="configGroupOverlay"></div>
                                }
                                return (
                                    <div key={parameterGroupIndex} className="nsConfigPrimitiveParameterGroupParameters">
                                    <h2>{parameterGroup.name} {optionalField}</h2>
                                    <div className="parameterGroup">
                                        {overlayGroup}
                                        <ul className="">
                                                <li className="">

                                                    {parameterGroup['parameter'] && parameterGroup['parameter'].map((parameter, parameterIndex) => {
                                                        let optionalField = '';
                                                        let displayField = '';
                                                        let defaultValue = parameter['default-value'] || '';
                                                        let isFieldHidden = (parameter['hidden'] && parameter['hidden'] == 'true') || false;
                                                        let isFieldReadOnly = (parameter['read-only'] && parameter['read-only'] == 'true') || false;
                                                        if (parameter.mandatory == mandatoryFieldValue) {
                                                            optionalField = <span className="required">*</span>
                                                        }
                                                        if (isFieldReadOnly) {
                                                            displayField = <div className='readonly'>{parameter.value || defaultValue}</div>
                                                        } else {
                                                            displayField = <input required={(parameter.mandatory == mandatoryFieldValue)} className="" disabled={inputIsDiabled} type="text" defaultValue={defaultValue} value={parameter.value} onChange={this.handleNsParamGroupParamChange.bind(this, parameterIndex, parameterGroupIndex, nsConfigPrimitiveIndex)} />
                                                        }
                                                        if (parameter.mandatory == mandatoryFieldValue) {
                                                            optionalField = <span className="required">*</span>
                                                        }
                                                        return (
                                                            <div key={parameterIndex} className="nsConfigPrimitiveParameters" style={{display: isFieldHidden ? 'none':'inherit'}}>
                                                                <ul>
                                                                {
                                                                    <li key={parameterIndex} className="">
                                                                        <label className={inputIsDiabled && 'disabled'} data-required={(parameter.mandatory == mandatoryFieldValue)}>
                                                                            {parameter.name}
                                                                            {displayField}
                                                                        </label>

                                                                    </li>
                                                                }
                                                                </ul>
                                                            </div>
                                                        )
                                                    })}
                                                </li>
                                            </ul>
                                        </div>
                                    </div>
                                );
                            })}
                            {nsConfigPrimitive['vnf-primitive-group'] && nsConfigPrimitive['vnf-primitive-group'].map((vnfGroup, vnfGroupIndex) => {
                                return (
                                    <div key={vnfGroupIndex} className="vnfParamGroup">
                                        <h2>{vnfGroup.name}</h2>

                                            {vnfGroup.inputs.map((inputGroup, inputGroupIndex) => {
                                                return (
                                                    <div key={inputGroupIndex}>
                                                        <h3>{inputGroup.name}</h3>
                                                        <ul className="parameterGroup">
                                                        {
                                                            inputGroup.parameter.map((input, inputIndex) => {
                                                                let optionalField = '';
                                                                let displayField = '';
																let defaultValue = input['default-value'] || '';
                                                                let isFieldHidden = (input['hidden'] && input['hidden'] == 'true') || false;
                                                                let isFieldReadOnly = (input['read-only'] && input['read-only'] == 'true') || false;
                                                                if (input.mandatory == mandatoryFieldValue) {
                                                                    optionalField = <span className="required">*</span>
                                                                }
                                                                if (isFieldReadOnly) {
                                                                    displayField = <div className='readonly'>{input.value || defaultValue}</div>
                                                                } else {
                                                                    displayField = <input required={(input.mandatory == mandatoryFieldValue)} className="" type="text" defaultValue={defaultValue} value={input.value} onChange={this.handleParamChange.bind(this, inputIndex, inputGroupIndex, vnfGroupIndex, nsConfigPrimitiveIndex)}/>
                                                                }
                                                                return (
                                                                    <li key={inputIndex} style={{display: isFieldHidden ? 'none':'inherit'}}>
                                                                        <label data-required={(input.mandatory == mandatoryFieldValue)}>
                                                                                {input.name}
                                                                                {displayField}
                                                                        </label>

                                                                    </li>
                                                                )
                                                            })
                                                        }
                                                        </ul>
                                                    </div>
                                                )
                                            })}

                                    </div>
                                )
                            })}
                            <div style={{marginTop: '1rem'}}>
                                * required
                            </div>
                            <Button label={nsConfigPrimitive.name} isLoading={this.state.isSaving}onClick={this.handleExecuteClick.bind(this, nsConfigPrimitiveIndex)} className="dark"/>
                        </TabPanel>
                    )
                );
            });
        }
    }

    handleSelect = (index, last) => {
        // console.log('Selected tab is', index, 'last index is', last);
    }
    render() {

        let tabList = [];
        let tabPanels = [];
        let isConfiguring = (this.props.data['config-status'] && this.props.data['config-status'] != 'configured') || false;
        let displayConfigStatus = isConfiguring ? '(Disabled - Configuring)': '';

        this.constructConfigPrimitiveTabs(tabList, tabPanels);

        return (
            <div className="nsConfigPrimitives">
                <div className="launchpadCard_title">
                  CONFIG-PRIMITIVES {displayConfigStatus}
                </div>
                <div className={isConfiguring ? 'configuring': '' + 'nsConfigPrimitiveTabs'}>
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


function deepCopy (toCopy, copyObj) {
    for (let k in toCopy) {
        switch (toCopy[k].constructor.name) {
            case "String":
                copyObj[k] = toCopy[k];
                break;
            case "Array":
                copyObj[k] = [];
                toCopy[k].map((v) => {
                    copyObj[k].push(v)
                });
                break;
            case "Object":
                deepCopy(toCopy[k], copyObj[k]);
                break;
            default:
                copyObj[k] = toCopy[k]
        }
    }
    return copyObj;
}
