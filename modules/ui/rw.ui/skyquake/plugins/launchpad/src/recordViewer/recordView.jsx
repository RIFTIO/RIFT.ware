
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import RecordNavigator from './recordNavigator.jsx';
import RecordCard from './recordCard.jsx';
import RecordDetails from './recordDetails.jsx';
import RecordViewStore from './recordViewStore.js';
import RecordViewActions from './recordViewActions.js';
import LaunchpadBreadcrumbs from '../launchpadBreadcrumbs.jsx';
import Utils from 'utils/utils.js';
import AppHeader from 'widgets/header/header.jsx';
import './recordViewer.scss';
export default class RecordView extends React.Component {
  constructor(props) {
    super(props);
    this.state = RecordViewStore.getState();
    this.state.showRecordDetails = false;
    RecordViewStore.listen(this.storeListener);
  }
  openLog() {
      var LaunchpadStore = require('../launchpadFleetStore.js')
      LaunchpadStore.getSysLogViewerURL('lp');
  }
  openAbout = () => {
    this.componentWillUnmount();
    let loc = window.location.hash.split('/');
    loc.pop();
    loc.pop();
    loc.push('lp-about');
    window.location.hash = loc.join('/');
  }
  openDebug = () =>  {
    this.componentWillUnmount();
    let loc = window.location.hash.split('/');
    loc.pop();
    loc.pop();
    loc.push('lp-debug');
    window.location.hash = loc.join('/');
  }
  storeListener = (state) => {
    this.setState(state);
  }
  componentWillUnmount = () => {
    this.state.socket.close();
    RecordViewStore.unlisten(this.storeListener);
  }
  componentDidMount() {
    let nsrRegEx = new RegExp("([0-9a-zA-Z-]+)\/detail");
    let nsr_id;
    try {
      console.log('NSR ID in url is', this.props.location.query);
      console.log(this.props)
      nsr_id =  this.props.location.query.id;
    } catch (e) {

    }
    RecordViewStore.getNSR(nsr_id);
    RecordViewStore.getRawNSR(nsr_id);
    RecordViewStore.getNSRSocket(nsr_id);
  }
  loadRecord = (record) => {
    RecordViewActions.loadRecord(record);
    RecordViewStore['getRaw' + record.type.toUpperCase()](record.id)
    RecordViewStore['get' + record.type.toUpperCase() + 'Socket'](record.id)
  }
  recordDetailsToggle = () => {
    this.setState({
      showRecordDetails: !this.state.showRecordDetails
    })
  }
  render() {
    let {location} = this.props;
    let html;
    let mgmtDomainName = window.location.hash.split('/')[2];
    let recordDetails = this.state.showRecordDetails || null;
    let nsrId = 0;
    let navItems = [{
      name: 'Viewport'
    },{
      name: 'COMPUTE TOPOLOGY',
      onClick: this.context.router.push.bind(this, {pathname:'/compute-topology', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
    }];

    if (location.query.sdnpresent == 'true') {
      navItems.push({
         name: 'NETWORK TOPOLOGY',
               onClick: this.context.router.push.bind(this, {pathname:'/network-topology', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
      });
    }

    let nav = <AppHeader nav={navItems} />
    if (this.state.showRecordDetails) {
      recordDetails = <RecordDetails isLoading={this.state.detailLoading} data={this.state.rawData} />
    }
    html = (
      <div className="app-body recordView">
      {nav}
        <div className="recordViewer">
          <i className="corner-accent top left"></i>
          <i className="corner-accent top right"></i>
          <div className="dashboardCard_wrapper recordPanels">
            <RecordNavigator activeNavID={this.state.recordID} nav={this.state.nav} loadRecord={this.loadRecord} isLoading={this.state.isLoading} />
            <RecordCard isLoading={this.state.cardLoading} type={this.state.recordType} data={this.state.recordData} recordDetailsToggleValue={this.state.showRecordDetails} vnfrs={this.state.vnfrs} navRef={this.state.nav} recordDetailsToggleFn={this.recordDetailsToggle} />
            {recordDetails}
          </div>
          <i className="corner-accent bottom left"></i>
          <i className="corner-accent bottom right"></i>
        </div>
      </div>
    );
    return html;
  }
}
RecordView.contextTypes = {
    router: React.PropTypes.object
};
