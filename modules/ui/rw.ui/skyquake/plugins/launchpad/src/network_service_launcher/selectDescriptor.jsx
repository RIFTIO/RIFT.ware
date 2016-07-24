
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React, {Component} from 'react';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import {Tab, Tabs, TabList, TabPanel} from 'react-tabs';
import CatalogItems from './catalogItems.jsx';
// import NetworkServiceStore from './launchNetworkServiceStore.js';
import NetworkServiceActions from './launchNetworkServiceActions.js';
import SkyquakeComponent from 'widgets/skyquake_container/skyquakeComponent.jsx';
//Data temporarily set through store and state for development
class SelectDescriptor extends Component {
  constructor(props, context) {
    super(props);
    this.Store = this.props.flux.stores.LaunchNetworkServiceStore;
  }
  componentWillReceiveProps(nextProps) {
  }
  render() {
    return (
      <DashboardCard showHeader={true} title={'1. Select NSD'} className={'selectDescriptor'}>
            <CatalogItems selectedNSDid={this.props.selectedNSDid} catalogs={this.props.nsd} onSelect={this.Store.descriptorSelected} />
      </DashboardCard>
    );
  }
}
SelectDescriptor.defaultProps = {
  nsd:[{

  }],
  vnfd:[{

  }],
  pnfd: [{

  }]
}

export default SkyquakeComponent(SelectDescriptor)
