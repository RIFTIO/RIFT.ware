
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import MonitoringParamsCarousel from '../monitoring_params/monitoringParamsCarousel.jsx';
import './vnfrCard.scss';
import VnfrCardNfviMetrics from './vnfrCardNfviMetrics.jsx';
class VnfrCard extends React.Component{
  constructor(props) {
    super(props);
  }
  render() {
    let html;
    let monitoring_params = <MonitoringParamsCarousel component_list={this.props.data["monitoring-param"]} slideno={this.props.slideno}/>;
    let nfviMetrics = this.props.data["nfvi-metrics"] ? <VnfrCardNfviMetrics metrics={this.props.data["nfvi-metrics"]} /> : <div style={{textAlign:'center', paddingTop:'2rem'}}>No NFVI Metrics Configured</div>;
    console.log(this.props.data)
    html = (
      <DashboardCard className="VNFRcard" showHeader={true} title={this.props.data["short-name"]}>
        {monitoring_params}
        {nfviMetrics}
      </DashboardCard>
    )
    return html;
  }
}

export default VnfrCard;
