/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import AppHeader from 'widgets/header/header.jsx';
import SdnAccountStore from '../launchpad_sdn_account/sdnAccountStore.js';
import CloudAccount from './cloudAccount.jsx';
import CloudAccountStore from './cloudAccountStore';
import AccountSidebar from '../account_sidebar/accountSidebar.jsx';
import './cloud-account.css';
import SkyquakeComponent from 'widgets/skyquake_container/skyquakeComponent.jsx';

class LaunchpadCloudAccount extends React.Component {
    constructor(props) {
        super(props);
        this.cloudName = this.props.routeParams.name;
        this.state = CloudAccountStore.getState();
        this.state.sdnAccounts = [];
        SdnAccountStore.getSdnAccounts();
        CloudAccountStore.listen(this.updateState);
        SdnAccountStore.listen(this.updateSdnAccount);
        if(this.cloudName && this.cloudName != 'create') {
            CloudAccountStore.getCloudAccountByName(this.cloudName)
            this.state.isEdit = true;
        } else {
            this.state.isLoading = false;
            this.state.isEdit = false;
        }
    }
    componentWillUnmount() {
        CloudAccountStore.unlisten(this.updateState);
        SdnAccountStore.unlisten(this.updateSdnAccount);
    }
    componentWillReceiveProps(props) {
        let cn = props.routeParams.name;
        if(cn && (cn != 'create')) {
            this.cloudName = cn;
            CloudAccountStore.getCloudAccountByName(this.cloudName);
            this.setState({isEdit: true});
        } else {
            CloudAccountStore.resetState();
            this.setState({isEdit: false});
        }
    }
    updateSdnAccount = (data) => {
        let sdns = data.sdnAccounts || [];
        //[{"name":"test","account-type":"mock","mock":{"username":"test"}}]
        let toSend = [
            {
                "label" : "Select an SDN Account",
                "value": false
            }
        ]
        sdns.map(function(sdn) {
            sdn.label=sdn.name;
            sdn.value = sdn.name
            toSend.push(sdn);
        });

        this.setState({
            sdnAccounts: toSend
        })
    }
    updateState = (state) => {
        this.setState(state);
    }
    render() {
        let html;
        let body;
        let title = "Launchpad: Add Cloud Account";
        if (this.props.isEdit) {
            title = "Launchpad: Edit Cloud Account";
        }
        if (this.props.isDashboard) {
            body = (<div>Edit or Create New Accounts</div>);
        } else {
             body = <CloudAccount {...this.props} store={CloudAccountStore} actions={this.props.actions} sdnAccounts={this.state.sdnAccounts} isEdit={this.state.isEdit} />
        }
        html = (<div className="cloud-account-wrapper">
                    <AppHeader title={title} isLoading={this.state.isLoading} />
                    <div className="flex">
                      <AccountSidebar/>
                      {body}
                    </div>
              </div>);
        return html;
    }
}
export default SkyquakeComponent(LaunchpadCloudAccount);
