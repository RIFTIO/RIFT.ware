
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import {Tab, Tabs, TabList, TabPanel} from 'react-tabs';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import MonitoringParamsCarousel from '../monitoring_params/monitoringParamsCarousel.jsx';
import VnfrCard from '../vnfr/vnfrCard.jsx';
import {LaunchpadCard, LpCardNfviMetrics, EpaParams, NsrPrimitiveJobList} from '../launchpad_card/launchpadCard.jsx';
import VnfrConfigPrimitives from '../launchpad_card/vnfrConfigPrimitives.jsx';
import NsrConfigPrimitives from '../launchpad_card/nsrConfigPrimitives.jsx';
import NsrScalingGroups from '../launchpad_card/nsrScalingGroups.jsx';
import LoadingIndicator from 'widgets/loading-indicator/loadingIndicator.jsx';
import NfviMetricBars from 'widgets/nfvi-metric-bars/nfviMetricBars.jsx';
import ParseMP from '../monitoring_params/monitoringParamComponents.js';
import PlacementGroupsInfo from './placementGroupsInfo.jsx';
export default class RecordCard extends React.Component {
  constructor(props) {
    super(props)
  }

  handleSelect = (index, last) => {
      // console.log('Selected tab is', index, 'last index is', last);
  }

  render(){
    let html;
    let content;
    let card;
    let cardData = {};
    let components = [];
    let configPrimitivesProps = {};
    let displayConfigPrimitives = false;
    let configPrimitiveComponent = null;
    let scalingGroupsProps = {};
    let displayScalingGroups = false;
    let scalingGroupComponent = null;

    let tabList = [];
    let tabPanels = [];

    switch(this.props.type) {
      case 'vnfr' :
        cardData = this.props.data[0];
        // Disabling config primitives for VNF
        configPrimitivesProps = [cardData];
        displayConfigPrimitives = cardData['config-primitives-present'];
        if (displayConfigPrimitives) {
          configPrimitiveComponent = (
            <div className="flex vnfrConfigPrimitiveContainer">
              <VnfrConfigPrimitives data={configPrimitivesProps} />
            {/* <NsrPrimitiveJobList jobs={cardData['config-agent-job']}/> */}
            </div>
          );
        }
        components = ParseMP.call(this, cardData["monitoring-param"], "vnfr-id");
        break;
      case 'nsr' :
        cardData = this.props.data.nsrs[0];
        configPrimitivesProps = cardData;
        scalingGroupsProps = cardData;
        displayConfigPrimitives = cardData['config-primitive'];
        displayScalingGroups = cardData['scaling-group-descriptor'] ? true : false;
        if (displayConfigPrimitives) {
          configPrimitiveComponent = (
            <div className="flex nsConfigPrimitiveContainer">
              <NsrConfigPrimitives data={configPrimitivesProps} />
              <NsrPrimitiveJobList jobs={cardData['config-agent-job']}/>
            </div>
          );
        }
        if (displayScalingGroups) {
          scalingGroupComponent = (
            <div className="flex nsrScalingGroupContainer">
              <NsrScalingGroups data={scalingGroupsProps} />
            </div>
          );
        }
        components = ParseMP.call(this, cardData["monitoring-param"], "vnfr-id");
        break;
    }
    let mgmt_interface = cardData["dashboard-url"];
    let mgmtHTML;
    let metricsAndParams = [];


    let nfviMetrics = <LpCardNfviMetrics data={cardData["nfvi-metrics"]} />;
    metricsAndParams.push(<div className="monitoringParams" key="mp">
                          {components.sort().map(function(c, k) {
                            return <div key={k} className="mpSlide">{c.title}{c.component}</div>
                          })}
                          </div>)
    metricsAndParams.push((<div key="nvfi" className="nfvi-metrics">
      { nfviMetrics }
      <EpaParams data={cardData["epa-params"]} />
    </div>))



    if(mgmt_interface) {
      mgmtHTML = <a href={mgmt_interface} target="_blank">Open Application Dashboard</a>;
    }
      if(this.props.isLoading) {
        html = <DashboardCard className="loading" showHeader={true} title={cardData["short-name"]}><LoadingIndicator size={10} show={true} /></DashboardCard>
      } else {
        let glyphValue = (this.props.recordDetailsToggleValue) ? "chevron-left" : "chevron-right";

        if (this.props.type == 'nsr') {
          tabList.push(
            <Tab key={cardData.id}>NS Data</Tab>
          )
        } else if (this.props.type == 'vnfr') {
          tabList.push(
            <Tab key={cardData.id}>VNF Data</Tab>
          )
        }

        tabPanels.push(
          <TabPanel key={cardData.id + '-panel'}>

              <div className="launchpadCard_title" style={{textAlign:'right'}}><span style={{float:'left'}}>MONITORING PARAMETERS</span>
                {mgmtHTML}
              </div>
              {metricsAndParams}
              <div className="cardSectionFooter">
              </div>
          </TabPanel>
        )


        if (this.props.type == 'nsr') {
          if (scalingGroupComponent) {
            tabList.push(
            <Tab key={cardData.id + '-sg'}>Scaling Groups</Tab>
          );

          tabPanels.push(
              <TabPanel key={cardData.id + '-sg-panel'}>
                  <div>
                    {scalingGroupComponent}
                  </div>
                  <div className="cardSectionFooter">
                  </div>
              </TabPanel>
            );
          }
          if(cardData.hasOwnProperty('vnfd-placement-group-maps')
             || cardData.hasOwnProperty('nsd-placement-group-maps')) {
            tabList.push(
               <Tab key='placement_groups'>
                Placement
               </Tab>
             );
            tabPanels.push(
               <TabPanel key='placement_groups_panel'>
                  <div>
                    <PlacementGroupsInfo nsr={cardData} navRef={this.props.navRef} />
                  </div>
              </TabPanel>
             );
          }
        }

        if (configPrimitiveComponent) {
          let primitivesTabTitle = '';
          if (this.props.type == 'nsr') {
            primitivesTabTitle = 'Service Primitive';
          } else if (this.props.type == 'vnfr') {
            primitivesTabTitle = 'Config Primitive'
          }

          tabList.push(
            <Tab key={cardData.id + '-cp'}>{primitivesTabTitle}</Tab>
          );

          tabPanels.push(
            <TabPanel key={cardData.id + '-cp-panel'}>
              <div className="configPrimitives">
                {configPrimitiveComponent}
              </div>
              <div className="cardSectionFooter">
              </div>
            </TabPanel>
          )
        }

        html = (
            <DashboardCard className="recordCard" showHeader={true} title={cardData["short-name"]}>
              <a onClick={this.props.recordDetailsToggleFn} className={"recordViewToggle " + (this.props.recordDetailsToggleValue ? "on": null)}><span className="oi" data-glyph={glyphValue} title="Toggle Details Panel" aria-hidden="true"></span></a>
              <Tabs onSelect={this.handleSelect}>
                <TabList>
                    {tabList}
                </TabList>
                {tabPanels}
              </Tabs>
            </DashboardCard>
        );
      }
    return html;
  }
}
RecordCard.defaultProps = {
  type: "default",
  data: {},
  isLoading: true
}
