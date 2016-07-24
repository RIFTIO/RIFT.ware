
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import LaunchpadCard from './launchpad_card/launchpadCard.jsx';
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';
import ScreenLoader from 'widgets/screen-loader/screenLoader.jsx';
import MPFilter from './monitoring-params-filter.jsx'
import NsCardPanel from './nsCardPanel/nsCardPanel.jsx';
import NsListPanel from './nsListPanel/nsListPanel.jsx';
import Crouton from 'react-crouton'
import AppHeader from 'widgets/header/header.jsx';
import Utils from 'utils/utils.js';
import './launchpad.scss';
let ReactCSSTransitionGroup = require('react-addons-css-transition-group');
var LaunchpadFleetActions = require('./launchpadFleetActions.js');
var LaunchpadFleetStore = require('./launchpadFleetStore.js');

export default class LaunchpadApp extends React.Component {
  constructor(props) {
    super(props);
    var self = this;
    this.state = LaunchpadFleetStore.getState();
    this.state.slideno = 0;
    this.handleUpdate = this.handleUpdate.bind(this);

  }
  componentDidMount() {
    LaunchpadFleetStore.getCatalog();
    LaunchpadFleetStore.getLaunchpadConfig();
    LaunchpadFleetStore.listen(this.handleUpdate);

    // Can not put a dispatch here it causes errors
    // LaunchpadFleetActions.validateReset();
    setTimeout(function() {
      LaunchpadFleetStore.openNSRSocket();
    },100);
  /*
  setTimeout(function() {
    consoleo.log("launchpad.componentDidMount openNsrListSocket");
    LaunchpadFleetStore.openNsrListSocket();
  },100);
    */

  }
  change(e) {

  }
  handleUpdate(data){
    this.setState(data);
  }
  closeError() {
    LaunchpadFleetActions.validateReset()
  }
  componentWillUnmount() {
    LaunchpadFleetStore.closeSocket();
    LaunchpadFleetStore.unlisten(this.handleUpdate);
  }

  handleOpenNsCard(nsr) {
    LaunchpadFleetActions.openNSRCard(nsr);
  }

  handleCloseNsCard(id) {
    LaunchpadFleetActions.closeNSRCard(id);
  }

  handleShowHideNsListPanelToggle() {
    this.setState({
      showNsListPanel: !this.state.showNsListPanel
    })
  }

  openNsrs() {
    return this.state.nsrs;
  }

  render () {
    var self = this;
    let mgmtDomainName = window.location.hash.split('/')[2];
    let navItems = [];
    if (this.state.isStandAlone) {
      navItems.push({
        name: 'ACCOUNTS',
      onClick: this.context.router.push.bind(this, {pathname:'/accounts'})
      })
    };
    if(!mgmtDomainName) {
      mgmtDomainName = 'dashboard';
    }
    if(mgmtDomainName.toUpperCase() == 'DASHBOARD' || mgmtDomainName.toUpperCase() == 'UNDEFINED') {
      mgmtDomainName = '';
    } else {
      mgmtDomainName = ' : ' + mgmtDomainName;
    }
    let nav = <AppHeader title={'LAUNCHPAD' + mgmtDomainName} nav={navItems} />

    return (
      <div className="app-body">
        <div className="lp_dashboard">
          <NsListPanel nsrs={self.state.nsrs}
            openedNsrIDs={self.state.openedNsrIDs}
            isVisible={self.state.isNsListPanelVisible}
            />
          <NsCardPanel nsrs={self.state.nsrs}
            openedNsrIDs={self.state.openedNsrIDs} />
        </div>
      </div>
    );
    }
}
LaunchpadApp.contextTypes = {
    router: React.PropTypes.object
};
LaunchpadApp.defaultProps = {
  // name: 'Loading...',
  // data: []
  openedNsrIDs: []
};
