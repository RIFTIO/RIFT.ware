/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import AppHeader from 'widgets/header/header.jsx';
import SdnAccountStore from '../launchpad_sdn_account/sdnAccountStore.js';
import CloudAccount from '../launchpad_cloud_account/cloudAccount.jsx';
import CloudAccountStore from '../launchpad_cloud_account/cloudAccountStore';
import AccountSidebar from '../account_sidebar/accountSidebar.jsx';
import './launchpadAccountDashboard.scss';

export default class LaunchpadAccountDashboard extends React.Component {
    constructor(props) {
        super(props);
        let CloudName = this.props.location.query.name;
        this.state = CloudAccountStore.getState();
        this.state.sdnAccounts = [];
        SdnAccountStore.getSdnAccounts();
        CloudAccountStore.listen(this.updateState);
        SdnAccountStore.listen(this.updateSdnAccount);
        if(CloudName) {
            CloudAccountStore.getCloudAccountByName(CloudName)
        } else {
            this.state.isLoading = false;
        }
    }
    componentWillUnmount(){
        CloudAccountStore.unlisten(this.updateState);
        SdnAccountStore.unlisten(this.updateSdnAccount);
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
    loadComposer = () => {
      let API_SERVER = require('utils/rw.js').getSearchParams(window.location).api_server;
      let auth = window.sessionStorage.getItem("auth");
      let mgmtDomainName = window.location.hash.split('/')[2];
        window.location.replace('//' + window.location.hostname + ':9000/index.html?api_server=' + API_SERVER + '&upload_server=' + window.location.protocol + '//' + window.location.hostname + '&clearLocalStorage' + '&mgmt_domain_name=' + mgmtDomainName + '&auth=' + auth);
    }
    render() {
        let html;
        let body;
        let title = "Launchpad: Add Cloud Account";
        if (this.props.edit) {
            title = "Launchpad: Edit Cloud Account";
        }
        html = (<div className="launchpad-account-dashboard">
                  <AppHeader title={title} isLoading={this.state.isLoading} />
                    <div className="flex">
                      <AccountSidebar/>
                      <div>Edit or Create New Accounts</div>
                    </div>
              </div>);
        return html;
    }
}
LaunchpadAccountDashboard.contextTypes = {
    router: React.PropTypes.object
};
