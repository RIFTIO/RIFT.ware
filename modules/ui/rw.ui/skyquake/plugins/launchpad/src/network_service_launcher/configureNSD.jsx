
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React, {Component} from 'react';
import ReactDOM from 'react-dom';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import { SelectOption } from './launchNetworkService.jsx';
import imgAdd from '../../node_modules/open-iconic/svg/plus.svg'
import imgRemove from '../../node_modules/open-iconic/svg/trash.svg'
export default class ConfigureNSD extends Component {
  constructor(props) {
    super(props);
  }
  render() {
    const { nsd, name, inputParameters, cloudAccounts, dataCenters, displayPlacementGroups, nsPlacementGroups, vnfPlacementGroups, ...props } = this.props;
    let html;
    let dataCentersHTML = null;
    let inputParametersHTML = null;
    let nsPlacementGroupsHTML = null;
    let vnfPlacementGroupsHTML = null;

    if (inputParameters && inputParameters.length > 0) {
      inputParametersHTML = inputParameters.map(function(input, i) {
        return (
                <div className="configure-nsd_section" key={i}>
                  <h3 className="launchpadCard_title">Input Parameters</h3>
                  <div className="inputControls">
                    <label>
                      <span> { input.label || input.xpath } </span>
                      <input type="text" onChange={props.onParametersUpdated.bind(self, i)} />
                    </label>
                  </div>
                </div>
        )
      })
    }

    if (nsPlacementGroups && nsPlacementGroups.length > 0 && displayPlacementGroups) {
      nsPlacementGroupsHTML = (
        <div className="configure-nsd_section">
          <h3 className="launchpadCard_title">NS Placement Groups</h3>
            {
              nsPlacementGroups.map(function(input, i) {
                return (
                  <div  key={i} className="configure-nsd_section-info">
                    <div className="placementGroup_description">
                      <div className="placementGroup_description-name">
                        <strong>{input.name}</strong> contains: {
                          input['member-vnfd'].map((m,i) => {
                            let s = m.name;
                            if(i>0) {
                              s = ', ' + m.name
                            };
                            return s;
                          })
                        }
                      </div>
                      <div><em>{input.requirement}</em></div>
                      <div><strong>Strategy:</strong> {input.strategy}</div>
                    </div>
                    <label>
                      <span> Availability Zone <span onClick={showInput} className="addInput"><img src={imgAdd} />Add</span> </span>
                      <div>
                      <input type="text" onChange={props.onNsPlacementGroupUpdate.bind(self, i, 'availability-zone')} />
                      <span onClick={hideInput} className="removeInput"><img src={imgRemove} />Remove</span>
                      </div>
                    </label>
                    <label>
                      <span> Affinity/Anti-affinity Server Group<span onClick={showInput} className="addInput"><img src={imgAdd} />Add</span></span>
                      <div>
                        <input type="text" onChange={props.onNsPlacementGroupUpdate.bind(self, i, 'server-group')} />
                        <span onClick={hideInput} className="removeInput"><img src={imgRemove} />Remove</span>
                      </div>
                    </label>
                    <div className="host-aggregate">
                      <label>
                        <span> Host Aggregates <span onClick={props.addNsHostAggregate.bind(self, i)} className="addInput"  ><img src={imgAdd} />Add</span></span>
                      {
                        input['host-aggregate'].length > 0 ?
                          input['host-aggregate'].map((h,j) => {
                            let key = h.key || '';
                            let value = h.value || '';
                            return (

                                  <div className="input_group" key={j}>
                                    <input type="text" onChange={props.onNsHostAggregateUpdate.bind(self, i, j, 'key')} placeholder="KEY" value={key} />
                                    <input type="text" onChange={props.onNsHostAggregateUpdate.bind(self, i, j, 'value')} placeholder="VALUE"  value={value} />
                                    <span onClick={props.removeNsHostAggregate.bind(self, i, j)} className="removeInput"><img src={imgRemove} />Remove</span>
                                  </div>
                            )
                          }) : ''
                      }
                      </label>
                    </div>
                    </div>
                );
              })
            }
       </div>
     );
    }
    if (vnfPlacementGroups && vnfPlacementGroups.length > 0 && displayPlacementGroups) {
      vnfPlacementGroupsHTML = vnfPlacementGroups.map(function(input, i) {
            return (
              <div className="configure-nsd_section" key={i}>
                <h3 className="launchpadCard_title">{input['vnf-name']} VNF Placement Group</h3>
                  <div className="configure-nsd_section-info">
                    <div className="placementGroup_description">
                      <div className="placementGroup_description-name">
                        <strong>{input.name}</strong> contains: {
                          input['member-vdus'].map((m,i) => {
                            let s = m['member-vdu-ref'];
                            if(i>0) {
                              s = ', ' + m['member-vdu-ref']
                            };
                            return s;
                          })
                        }
                      </div>
                      <div><em>{input.requirement}</em></div>
                      <div><strong>Strategy</strong>: {input.strategy}</div>
                    </div>
                    <label>
                      <span> Availability Zone <span onClick={showInput} className="addInput"><img src={imgAdd} />Add</span></span>
                      <div>
                        <input type="text" onChange={props.onVnfPlacementGroupUpdate.bind(self, i, 'availability-zone')} />
                           <span onClick={hideInput} className="removeInput"><img src={imgRemove} />Remove</span>
                      </div>
                    </label>
                    <label>
                      <span> Affinity/Anti-affinity Server Group<span onClick={showInput} className="addInput"><img src={imgAdd} />Add</span></span>
                      <div>
                        <input type="text" onChange={props.onVnfPlacementGroupUpdate.bind(self, i, 'server-group')} />
                         <span onClick={hideInput} className="removeInput"><img src={imgRemove} />Remove</span>
                      </div>
                    </label>
                    <div className="host-aggregate">
                    <label>
                      <span> Host Aggregates <span onClick={props.addVnfHostAggregate.bind(self, i)} className="addInput"><img src={imgAdd} />Add</span></span>
                      {
                        input['host-aggregate'].length > 0 ?
                          input['host-aggregate'].map((h,j) => {
                            let key = h.key || '';
                            let value = h.value || '';
                            return (

                                  <div className="input_group" key={j}>
                                    <input type="text" onChange={props.onVnfHostAggregateUpdate.bind(self, i, j, 'key')} placeholder="KEY" value={key} />
                                    <input type="text" onChange={props.onVnfHostAggregateUpdate.bind(self, i, j, 'value')} placeholder="VALUE"  value={value} />
                                    <span onClick={props.removeVnfHostAggregate.bind(self, i, j)} className="removeInput"><img src={imgRemove} />Remove</span>
                                  </div>

                            )
                          }) : ''
                      }
                      </label>
                    </div>
                    </div>
                </div>
            );
          })
    }
    if (dataCenters > 0) {
      dataCentersHTML = (
        <label>Select Cloud Account
          <SelectOption options={dataCenters} onChange={props.handleSelectDataCenter} />
        </label>
      )
    }
    html = (
        <DashboardCard showHeader={true} title={'3. Configure NSD'} className={'configure-nsd'}>
          <div className="configure-nsd_section">
            <h3 className="launchpadCard_title">Instance</h3>
            <div className="inputControls">
              <label>
                Name
                <input type="text" pattern="^[a-zA-Z0-9_]*$" style={{textAlign:'left'}} onChange={props.updateName} value={name}/>
              </label>
              <label>Select Cloud Account
                <SelectOption options={cloudAccounts} onChange={props.handleSelectCloudAccount} />
              </label>
              {
                dataCentersHTML
              }
            </div>
          </div>
          {
            inputParametersHTML
          }
          {
            nsPlacementGroupsHTML
          }
          {
            vnfPlacementGroupsHTML
          }
        </DashboardCard>
    )
    return html;
  }
}
function showInput(e){
  let target = e.target;
  if(target.parentElement.classList.contains("addInput")) {
    target = target.parentElement;
  }
  target.style.display = 'none';
  target.parentElement.nextElementSibling.style.display = 'flex';
  // e.target.parentElement.nextElementSibling.children[1].style.display = 'initial';
}
function hideInput(e){
  let target = e.target;
  if(target.parentElement.classList.contains("removeInput")) {
    target = target.parentElement;
  }
  target.parentElement.style.display = 'none';
  target.parentElement.previousElementSibling.children[1].style.display = 'initial';
  target.previousSibling.value = '';
}
ConfigureNSD.defaultProps = {
  data: []
}
