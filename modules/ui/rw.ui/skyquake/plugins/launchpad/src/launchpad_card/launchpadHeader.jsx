
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import UpTime from 'widgets/uptime/uptime.jsx';
import launchpadFleetStore from '../launchpadFleetStore.js';
import launchpadFleetActions from '../launchpadFleetActions';
import LaunchpadOperationalStatus from 'widgets/operational-status/launchpadOperationalStatus.jsx';
import LaunchpadCardMgmtInterfaces from './launchpadCardMgmtInterfaces.jsx';
import LaunchpadCardCloudAccount from './launchpadCardCloudAccount.jsx';
import { Link } from 'react-router';

class LaunchpadHeader extends React.Component {
  constructor(props) {
    super(props);
    this.state = {};
    this.state.displayStatus = false;
    this.state.isLoading = (this.props.nsr["operational-status"] == "running") ? false : true;
    this.state.displayInterfacePanel = false;
    this.state.displayCloudAccount = false;
    this.state.failed = false;
    this.deleteLaunchpad = this.deleteLaunchpad.bind(this);
    this.openDashboard = this.openDashboard.bind(this);
    this.openConsole = this.openConsole.bind(this);
    this.setStatus = this.setStatus.bind(this);
  }
  //TODO Do not setState within nested component like this. Find a way to use props.
  doneLoading = (failed) => {
  this.setState({
      isLoading: false,
      failed: failed
    });
  }
  //TODO instead of calling the store method, an action should be emitted and the store should respond.
  deleteLaunchpad(e) {
    if (window.confirm("Preparing to delete "+ this.props.name + ".  Are you sure you want to delete this NSR?")) {
      launchpadFleetStore.deleteNsrInstance(this.props.id);
      launchpadFleetActions.deletingNSR(this.props.id);
    }
  }
  openDashboard(nsr_id) {
    window.location.href = '//' + window.location.hostname + ':' + window.location.port + '/index.html' + window.location.search + window.location.hash + '/' + nsr_id + '/detail';
    launchpadFleetStore.closeSocket();
  }
  openConsole() {
    console.log('open console clicked');
  }
  setStatus() {
    var status;
    if(this.props.isActive) {
      status = "DISABLED"
    } else {
      status = "ENABLED"
    }
    launchpadFleetStore.setNSRStatus(this.props.nsr["id"], status);
  }
  openInterfacePanel = () => {
    this.setState({
      displayStatus: false,
      displayInterfacePanel: !this.state.displayInterfacePanel,
      displayCloudAccount: false
    })
     // this.closeHeaderViews();
  }
  openStatus = () => {
    this.setState({
      displayInterfacePanel: false,
      displayStatus: !this.state.displayStatus,
      displayCloudAccount: false
    });
    // this.closeHeaderViews();
  }
  openCloudAccountPanel = () => {
    let nsrCloudAccount = this.props.nsr['cloud-account'];
    this.setState({
      displayStatus: false,
      displayInterfacePanel: false,
      displayCloudAccount: !this.state.displayCloudAccount
    })
  }
  render() {
    let self = this;
    let failed = this.state.failed;
    let nsrCreateTime = this.props.nsr['create-time'];
    //let nsrUpDuration = (nsrCreateTime)
    //  ? Math.floor((new Date() / 1000)) - nsrCreateTime
    //  : null;
    let isLoading =  this.state.isLoading || failed;
    let toggleStatus = isLoading ? '' :
      (
        <h3 className="launchpadCard_header-link">
          <a onClick={this.openStatus} title="Open History" className={self.state.displayStatus ? 'activeIcon' : 'inActiveIcon'}>
            <span className="oi" data-glyph="clock" aria-hidden="true">
            </span>
          </a>
        </h3>
      );
    let headerClassName = "launchpadCard_header";
    // debugger;
    headerClassName += (failed ? ' failed' : '');
    let sdnpresent = this.props.nsr['sdn-account'] ? true: false;
    return (
      <header className={headerClassName}>
        <div className={"launchpadCard_header-title " + (this.props.isActive ? '' : 'launchpadCard_header-title-off') + (failed ? ' failed' : '')} >
            <h3 className="launchpadCard_header-link">
              {
                isLoading ?
                            <a title="Open Viewport Dashboard" style={{cursor:'default'}}>
                              {this.props.name}
                            </a>
                          : <Link to={{pathname: '/viewport', query: {id: this.props.nsr.id,
                              sdnpresent: sdnpresent}}}
                              title="Open Viewport Dashboard">
                              {this.props.name}
                              <span className="oi" data-glyph="external-link" aria-hidden="true"></span>
                            </Link>
              }
            </h3>
          <div className="launchpadCard_header-actions">
            <h3>
              {isLoading ? this.props.nsr["operational-status"] : this.props.nsr['config-status'] != 'configured' ? this.props.nsr['config-status'] : 'Active' }
            </h3>
            <h3 style={{display: isLoading ? 'none' : 'inherit'}}>
                <UpTime initialTime={nsrCreateTime} run={true} />
            </h3>
            <h3 className="launchpadCard_header-link" style={{display: this.props.nsr['cloud-account'] ? 'inherit' : 'none'}}>
              <a onClick={this.openCloudAccountPanel} title="Cloud Account" className={self.state.displayCloudAccount ? 'activeIcon' : 'inActiveIcon'}>
                <span className="oi" data-glyph="cloud" aria-hidden="true"></span>
              </a>
            </h3>
            <h3 className="launchpadCard_header-link" style={{'display': !isLoading ? 'inherit' : 'none'}}>
                <a onClick={this.openInterfacePanel} title="View Management Interfaces" className={self.state.displayInterfacePanel ? 'activeIcon' : 'inActiveIcon'}>
                  <span className="oi" data-glyph="monitor" aria-hidden="true">
                  </span>
                </a>
            </h3>
            {toggleStatus}
            <h3 className="launchpadCard_header-link" style={{display: 'inherit'}}>
              <a onClick={this.deleteLaunchpad} title="Delete">
                <span className="oi" data-glyph="trash" aria-hidden="true"></span>
              </a>
            </h3>
          </div>
        </div>
        <div className="launchpadCard_header-status">
        <LaunchpadOperationalStatus className="launchpadCard_header-operational-status" loading={isLoading} doneLoading={this.doneLoading} display={this.state.displayStatus} currentStatus={this.props.nsr["operational-status"]} status={this.props.nsr["rw-nsr:operational-events"]} />
        <LaunchpadCardMgmtInterfaces display={self.state.displayInterfacePanel} interfaces={this.props.nsr["dashboard-urls"]} className="launchpadCard_header-vnfr_management-links" />
        <LaunchpadCardCloudAccount display={self.state.displayCloudAccount} nsr={this.props.nsr} className="launchpadCard_header-vnfr_management-links" />
        </div>
      </header>
    )
  }
};
LaunchpadHeader.propTypes = {
  name: React.PropTypes.string
 };
LaunchpadHeader.defaultProps = {
  name: 'Loading...Some Name'
};

export default LaunchpadHeader;
