
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React, {Component} from 'react';
import SelectDecriptor from './selectDescriptor.jsx';
import ConfigureNSD from './configureNSD.jsx';
import SLAParams from './specifySLAParameters.jsx';
import LaunchNetworkServiceStore from './launchNetworkServiceStore.js';
import LaunchNetworkServiceActions from './launchNetworkServiceActions.js';
import Button from 'widgets/button/rw.button.js';
import './launchNetworkService.scss';
import Crouton from 'react-crouton'
import 'style/common.scss'
import '../launchpad.scss'
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';
import ScreenLoader from 'widgets/screen-loader/screenLoader.jsx';
import AppHeader from 'widgets/header/header.jsx';
// import AppHeaderActions from 'widgets/header/headerActions.js';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import { browserHistory } from 'react-router';
import SkyquakeComponent from 'widgets/skyquake_container/skyquakeComponent.jsx';
let ReactCSSTransitionGroup = require('react-addons-css-transition-group');
let API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;

let editNameIcon = require("style/img/svg/launch-fleet-icn-edit.svg")



class LaunchNetworkService extends Component {
  constructor(props) {
    super(props);
    this.Store = this.props.flux.stores.hasOwnProperty('LaunchNetworkServiceStore') ? this.props.flux.stores.LaunchNetworkServiceStore : this.props.flux.createStore(LaunchNetworkServiceStore);
    this.state = this.Store.getState();
    this.state.validate = false;
    this.handleUpdate = this.handleUpdate.bind(this);
    this.updateName = this.updateName.bind(this);
    this.handleSave = this.handleSave.bind(this);
    this.Store.listen(this.handleUpdate);
  }
  evaluateLaunch = (e) => {
    if (e.keyCode == 13) {
      this.handleSave(true, e);
      e.stopPropagation();
    }
  }
  openLog() {
      var LaunchpadStore = require('../launchpadFleetStore.js')
      LaunchpadStore.getSysLogViewerURL('lp');
  }
  openAbout() {
    let loc = window.location.hash.split('/');
    loc.pop();
    this.Store.resetView();
    loc.push('lp-about');
    window.location.hash = loc.join('/');
  }
  openDebug() {
    let loc = window.location.hash.split('/');
    loc.pop();
    this.Store.resetView();
    loc.push('lp-debug');
    window.location.hash = loc.join('/');
  }
  componentDidMount() {
    let self = this;
    this.Store.getCatalog();
    this.Store.getLaunchpadConfig();
    this.Store.getCloudAccount(function() {
      self.Store.getDataCenters();
    });
  }
  componentWillUnmount() {
    this.Store.unlisten(this.handleUpdate);
  }
  handleCancel = (e) => {
    e.preventDefault();
    this.props.router.push({pathname:''});
  }
  handleUpdate(data) {
    this.setState(data);
  }
  isOpenMano = () => {
    return this.state.selectedCloudAccount['account-type'] == 'openmano';
  }
  updateName(event) {
    let name = event.target.value;
    this.Store.nameUpdated(name);
  }
  handleSave(launch, e) {
    let self = this;
    e.preventDefault();
    if (this.state.name == "") {
        self.props.actions.showNotification('Please name the network service');
      return;
    }
    if (!this.state.name.match(/^[a-zA-Z0-9_]*$/g)) {
      self.props.actions.showNotification('Spaces and special characters except underscores are not supported in the network service name at this time');
      return;
    }
    if (this.isOpenMano() && (this.state.dataCenterID == "" || !this.state.dataCenterID)) {
         self.props.actions.showNotification("Please enter the Data Center ID");
      return;
    }
    // LaunchNetworkServiceStore.resetView();
    this.Store.saveNetworkServiceRecord(this.state.name, launch);
  }
  setValidate() {
    this.resetValidate = true;
  }
  handleSelectCloudAccount = (e) => {
    let cloudAccount = e;
    this.Store.updateSelectedCloudAccount(cloudAccount);
  }
  handleSelectDataCenter = (e) => {
    let dataCenter = e;
    this.Store.updateSelectedDataCenter(dataCenter);
  }
  inputParametersUpdated = (i, e) => {
    this.Store.updateInputParam(i, e.target.value)
  }
  //ns pg
  nsPlacementGroupUpdate = (pg, key, e) => {
    this.Store.nsPlacementGroupUpdate(pg, key, e.target.value)
  }
  nsHostAggregateUpdate = (pg, ha, key, e) => {
    this.Store.nsHostAggregateUpdate(pg, ha, key, e.target.value)
  }
  addNsHostAggregate = (pg, e) => {
    e.preventDefault();
    e.stopPropagation();
    this.Store.addNsHostAggregate(pg)
  }
  removeNsHostAggregate = (pg, ha, e) => {
    e.preventDefault();
    e.stopPropagation();
    this.Store.removeNsHostAggregate(pg, ha)
  }
  //end ns pg
  //vnf pg
  vnfPlacementGroupUpdate = (pg, key, e) => {
    this.Store.vnfPlacementGroupUpdate(pg, key, e.target.value)
  }
  vnfHostAggregateUpdate = (pg, ha, key, e) => {
    this.Store.vnfHostAggregateUpdate(pg, ha, key, e.target.value)
  }
  addVnfHostAggregate = (pg, e) => {
    e.preventDefault();
    e.stopPropagation();
    this.Store.addVnfHostAggregate(pg)
  }
  removeVnfHostAggregate = (pg, ha, e) => {
    e.preventDefault();
    e.stopPropagation();
    this.Store.removeVnfHostAggregate(pg, ha)
  }
  //end vnf pg
  render() {
    let name = this.state.name;
    var self = this;
    let mgmtDomainName = window.location.hash.split('/')[2];
    let navItems = [{
      name: 'DASHBOARD',
      onClick: function() {
          window.history.back(-1);
        }
      },
      {
        'name': 'Instantiate'
      }
    ];
    let dataCenterID = this.state.dataCenterID;
    let isOpenMano = this.isOpenMano();
    let isStandAlone = this.state.isStandAlone;
    let showCrouton = this.state.validate && this.state.validate.message;
    let message = this.state.validate.message;
    let CloudAccountOptions = [];
    let DataCenterOptions = [];
    let thirdPanel = null;
    //Build CloudAccountOptions
    CloudAccountOptions = this.state.cloudAccounts.map(function(ca, index) {
      return {
        label: ca.name,
        value: ca
      }
    });
    if (isOpenMano) {
        //Build DataCenter options
        let dataCenter = this.state.dataCenters[this.state.selectedCloudAccount.name];
        if(dataCenter){
            DataCenterOptions = dataCenter.map(function(dc, index) {
              return {
                label: dc.name,
                value: dc.uuid
              }
            });
        }
      }
    // if (this.state.hasConfigureNSD) {
    if (true) {
      thirdPanel = <ConfigureNSD
        nsd={this.state.nsd[0]}
        name={this.state.name}
        updateName={this.updateName}
        cloudAccounts={CloudAccountOptions}
        displayPlacementGroups={this.state.displayPlacementGroups}
        handleSelectCloudAccount={this.handleSelectCloudAccount}
        dataCenters={DataCenterOptions}
        handleSelectDataCenter = {this.handleSelectDataCenter}
        inputParameters={this.state['input-parameters']}
        onParametersUpdated={this.inputParametersUpdated}
        nsPlacementGroups={this.state['ns-placement-groups']}
        onNsPlacementGroupUpdate={this.nsPlacementGroupUpdate}
        onNsHostAggregateUpdate = {this.nsHostAggregateUpdate}
        removeNsHostAggregate = {this.removeNsHostAggregate}
        addNsHostAggregate = {this.addNsHostAggregate}
        vnfPlacementGroups={this.state['vnf-placement-groups']}
        onVnfPlacementGroupUpdate={this.vnfPlacementGroupUpdate}
        onVnfHostAggregateUpdate = {this.vnfHostAggregateUpdate}
        removeVnfHostAggregate = {this.removeVnfHostAggregate}
        addVnfHostAggregate = {this.addVnfHostAggregate}
        />
    }
    let html = (
      <form className={"app-body create-fleet"} onKeyDown={this.evaluateLaunch}>
      <AppHeader/>
        <ScreenLoader show={this.state.isLoading}/>
          <div className="launchNetworkService">
            <div className="dashboardCard_wrapper launchNetworkService_panels">
              <SelectDecriptor nsd={this.state.nsd} vnfd={this.state.vnfd} selectedNSDid={this.state.selectedNSDid}/>
              <SLAParams data={this.state.sla_parameters}/>
              {thirdPanel}
            </div>
            <div className="launchNetworkService_controls">
              <Button label="Cancel" onClick={this.handleCancel} onKeyDown={this.evaluateLaunch} className="light"/>
              {/*<Button label="Save" onClick={this.handleSave.bind(this, false)} className="light" loadingColor="black" isLoading={this.state.isSaving}/>*/}
              <Button label="Launch" isLoading={this.state.isSaving} onClick={this.handleSave.bind(self, true)} className="dark"  type="submit"/>
            </div>
          </div>
      </form>
    )
    return html
  }
}


export class SelectOption extends React.Component {
  constructor(props){
    super(props);
  }
  handleOnChange = (e) => {
    this.props.onChange(JSON.parse(e.target.value));
  }
  render() {
    let html;
    html = (
      <select className={this.props.className} onChange={this.handleOnChange}>
        {
          this.props.options.map(function(op, i) {
            return <option key={i} value={JSON.stringify(op.value)}>{op.label}</option>
          })
        }
      </select>
    );
    return html;
  }
}
SelectOption.defaultProps = {
  options: [],
  onChange: function(e) {
    console.dir(e)
  }
}

export default SkyquakeComponent(LaunchNetworkService);
