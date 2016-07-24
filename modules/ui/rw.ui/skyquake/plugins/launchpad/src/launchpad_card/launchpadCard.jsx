
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import './launchpad_card.scss';
import LaunchpadNSInfo from './launchpadNSInfo.jsx';
import LaunchpadHeader from './launchpadHeader.jsx';
import MonitoringParamsCarousel from '../monitoring_params/monitoringParamsCarousel.jsx';
import LaunchpadControls from './launchpadControls.jsx';
import NfviMetricBars from 'widgets/nfvi-metric-bars/nfviMetricBars.jsx';
import MetricBarGroup from 'widgets/metric-bars/metricBarGroup.jsx';
import LoadingIndicator from 'widgets/loading-indicator/loadingIndicator.jsx';
import RecordViewStore from '../recordViewer/recordViewStore.js';
import TreeView from 'react-treeview';

import circleXImage from '../../node_modules/open-iconic/svg/circle-x.svg';


class LaunchpadCard extends React.Component {
  constructor(props) {
    super(props);
  }
  shouldComponentUpdate(nextProps) {
    return true;
  }
  openLaunch = () => {
    this.context.router.push({pathname:'instantiate'});
  }
  render() {
    let html;
    let metrics = '';
    let monitoring_params_data;
    let deleting = false;
    let metricsAndParameters = [];
    let carousel = ''
    let closeCardAction = this.props.closeButtonAction;

    if(this.props.nsr && this.props.nsr.data) {
      metrics = this.props.nsr.data.map((info, index)=> {
          return (<LaunchpadNSInfo  key={index} name={info.name} data={info.data}/>)
      });
    }
    // debugger
    if(this.props.nsr && this.props.nsr["monitoring-param"]) {
      monitoring_params_data = this.props.nsr["monitoring-param"];
    } else {
      monitoring_params_data = null;
    }

    if (this.props.nsr && this.props.nsr.deleting) {
      deleting = true;
    }

    if(true) {
      // metricsAndParameters.push(<LaunchpadControls controlSets={this.props.nsr.nsControls} />)
      if (this.props.nsr) {
        if (this.props.nsr["nfvi-metrics"]) {
          metricsAndParameters.push((
                                     <div className="nfvi-metrics" key="nfviMetrics">
                                      <LpCardNfviMetrics key="nfvi-metrics" name="NFVI METRICS" id={this.props.id} data={this.props.nsr["nfvi-metrics"]} />
                                     </div>
                                     ))
        }
        if (this.props.nsr["epa-params"]) {
          metricsAndParameters.push(<EpaParams key="epa-params" data={this.props.nsr["epa-params"]} />);
        }
      }
      carousel = <MonitoringParamsCarousel component_list={monitoring_params_data} slideno={this.props.slideno}/>;
    }

    if(this.props.create){
      html = (
        <DashboardCard>
          <div className={'launchpadCard_launch'} onClick={this.openLaunch} style={{cursor:'pointer'}}>
            <img src={require("style/img/launchpad-add-fleet-icon.png")}/> Instantiate Network Service </div>
        </DashboardCard>
        );
    } else {
      let self = this;
      let closeButton = (
            <a onClick={self.props.closeButtonAction}
              className={"close-btn"}>
              <img src={circleXImage} title="Close card" />
              </a>
      );

      html = (
        <DashboardCard className={'launchpadCard'} closeCard={closeButton}>
          <LaunchpadHeader nsr={this.props.nsr} name={this.props.name} isActive={this.props.isActive} id={this.props.id}/>
          {
          deleting ?
          <div className={'deletingIndicator'}>
            <LoadingIndicator size={10} show={true} />
          </div>
          :
          <div className="launchpadCard_content">
            <div className="launchpadCard_title">
              NSD: {this.props.nsr.nsd_name}
            </div>
            {carousel}
            {metricsAndParameters}
          </div>
          }
        </DashboardCard>
      );
    }
    return html;
  }
}
LaunchpadCard.contextTypes = {
    router: React.PropTypes.object
};
LaunchpadCard.propTypes = {
  nsr: React.PropTypes.object,
  isActive: React.PropTypes.bool,
  name: React.PropTypes.string
 };
LaunchpadCard.defaultProps = {
  name: 'Loading...',
  data: {},
  isActive: false
};

export class LpCardNfviMetrics extends React.Component {
  constructor(props) {
    super(props)
  }
  convertToArray(o){
    let a = [];
    Object.keys(o).map(function(k){
      a.push(o[k]);
    });
    return a;
  }
  render() {
    let mets = this.props.data;
    let self = this;
    let html = (
      <div style={{overflow: 'hidden'}}>
        <div className="launchpadCard_title">
          NFVI-METRICS
        </div>
        <div className="metricBars">
        { (mets && mets.length > 0 ) ? mets.map(function(m,i) {
          return <MetricBarGroup key={i} index={i} title={m.title} lp_id={self.props.id} data={m.data.sort(function(a,b){return a.id > b.id})} />
        }) : <div className="empty"> NO NFVI METRICS CONFIGURED</div>}
        </div>
      </div>
    )
    return html;
  }
}
export class EpaParams extends React.Component {
  constructor(props) {
    super(props)
  }
  render() {
    let metrics =[];
    let epa = this.props.data;
    let count = 0;
    for(let k in epa) {
      let epaHTMLsub = [];
      let epaHTML;
      epaHTMLsub.push(buildParams(epa[k], count));
      epaHTML = (
        <li key={count + k}>
          <h1>{k}</h1>
          {
            epaHTMLsub
          }
        </li>
      );
      metrics.push(epaHTML);
      count++;
    }
    function buildParams(epa, index) {
      let html = [];
      let tCount = 0;
      let checkForTotal = function checkForTotal(epa, html, i) {
        for (let k in epa) {
          if("total" in epa[k]) {
            html.push(<dd key={"total" + tCount + k + i}>{k} : {epa[k].total} {(epa[k].total>1) ? 'vms' : 'vm'}</dd>)
          } else {
            html.push(<dt key={tCount + k + i}>{k}</dt>)
            checkForTotal(epa[k], html, tCount)
          }
          tCount++;
        }
      }
      checkForTotal(epa, html, index)
      return (<dl key={"subTop-" + index}>{html}</dl>);
    }
    let display = (<ul>
              {metrics}
            </ul>)
    // metrics = false;
    if(metrics.length == 0) display = <div className="empty">NO EPA PARAMS CONFIGURED</div>
    let html = (
      <div style={{overflow: 'hidden'}}>
        <div className="launchpadCard_title">
          EPA-PARAMS
        </div>
        <div className={"launchpadCard_data-list" + ' ' + 'EPA-PARAMS'}>
          {display}
        </div>
      </div>
    )
    return html;
  }
}


export class NsrPrimitiveJobList extends React.Component {
  constructor(props) {
    super(props)
  }
  render () {
    let tree = null;
    tree = this.props.jobs.sort(function(a,b){
      return b['job-id'] > a['job-id'];
    }).map(function(job, jindex) {
          let nsrJobLabel = job['job-name'] ? job['job-name'] + ': ' + job['job-status'] : "NSR Job ID:" + job['job-id'] + ': ' + job['job-status']
          return (
              <TreeView key={jindex} nodeLabel={nsrJobLabel} className="nsrJob" defaultCollapsed={true}>
                <h4>NSR Job Name: {job['job-name']}</h4>
                <h4>NSR Job ID: {job['job-id']}</h4>
                <h4>NSR Job Status: {job['job-status']}</h4>
                  {job.vnfr ?
                  <TreeView defaultCollapsed={true} className="vnfrJobs" nodeLabel="VNFR Jobs">
                    {job.vnfr.map(function(v, vindex) {
                      return (
                        <TreeView key={vindex} nodeLabel="VNFR Job">
                          <h5>VNFR ID: {v.id}</h5>
                          <h5>VNFR Job Status: {v['vnf-job-status']}</h5>
                            {v.primitive && v.primitive.map((p, pindex) => {
                              return (
                                <div key={pindex}>
                                  <div>Name: {p.name}</div>
                                  <div>Status: {p["execution-status"]}</div>
                                </div>
                              )
                            })}
                        </TreeView>
                      )
                    })}
                  </TreeView>
                  :null}
              </TreeView>
          )
        });
    let html = (
      <div  className="nsConfigPrimitiveJobList">
        <div className="launchpadCard_title">
          JOB-LIST
        </div>
        <div className="listView">
          <div>
            {tree}
          </div>
        </div>
      </div>
    );
    return html;
  }
}
NsrPrimitiveJobList.defaultProps = {
  jobs: []
}

export default LaunchpadCard
