/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import AppHeader from 'widgets/header/header.jsx';
import ConfigAgentAccount from './configAgentAccount.jsx';
import ConfigAgentAccountStore from './configAgentAccountStore';
import ConfigAgentAccountActions from './configAgentAccountActions';
import AccountSidebar from '../account_sidebar/accountSidebar.jsx';
import React from 'react';
import '../launchpad_cloud_account/cloud-account.css';
export default class LaunchpadConfigAgentAccount extends React.Component {
    constructor(props) {
        super(props);
        this.configName = this.props.routeParams.name;
        this.state = ConfigAgentAccountStore.getState();
        ConfigAgentAccountStore.listen(this.updateState);
        if(this.configName && this.configName != 'create') {
            ConfigAgentAccountStore.getConfigAgentAccount(this.configName)
            this.state.isEdit = true;
        } else {
            this.state.isLoading = false;
            this.state.isEdit = false;
        }
    }
    componentWillReceiveProps(props) {
        let cn = props.routeParams.name;
        if(cn && (cn != 'create')) {
            this.configName = cn;
            ConfigAgentAccountStore.getConfigAgentAccount(this.configName);
            this.setState({isEdit: true});
        } else {
            ConfigAgentAccountStore.resetState();
            this.setState({isEdit: false});
        }
    }
    componentWillUnmount() {
        ConfigAgentAccountStore.unlisten(this.updateState);
    }
    updateState = (state) => {
        this.setState(state);
    }
    render() {
        let html;
        let body;
        let title = "Launchpad: Add Config Agent Account";
        if (this.props.edit) {
            title = "Launchpad: Edit Config Agent Account";
        }
        if (this.props.isDashboard) {
            body = (<div>Edit or Create a New Accounts</div>);
        } else {
             body = <ConfigAgentAccount {...this.props} store={ConfigAgentAccountStore} actions={ConfigAgentAccountActions} edit={this.state.isEdit} />
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

LaunchpadConfigAgentAccount.contextTypes = {
    router: React.PropTypes.object
};