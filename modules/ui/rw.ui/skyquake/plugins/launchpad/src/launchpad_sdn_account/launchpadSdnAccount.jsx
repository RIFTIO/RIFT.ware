/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import AppHeader from 'widgets/header/header.jsx';
import SdnAccount from './sdnAccount.jsx';
import SdnAccountStore from './sdnAccountStore';
import SdnAccountActions from './sdnAccountActions';
import AccountSidebar from '../account_sidebar/accountSidebar.jsx';
import '../launchpad_cloud_account/cloud-account.css';
export default class LaunchpadSdnAccount extends React.Component {
    constructor(props) {
        super(props);
        this.sdnName = this.props.routeParams.name;
        this.state = SdnAccountStore.getState();
        SdnAccountStore.listen(this.updateState);
        if(this.sdnName && this.sdnName != 'create') {
            SdnAccountStore.getSdnAccount(this.sdnName)
            this.state.isEdit = true;
        } else {
            this.state.isLoading = false;
            this.state.isEdit = false;
        }
    }
    componentWillReceiveProps(props) {
        let cn = props.routeParams.name;
        if(cn  && (cn != 'create')) {
            this.sdnName = cn;
            SdnAccountStore.getSdnAccount(this.sdnName);
            this.setState({isEdit: true});
        } else {
            SdnAccountStore.resetState();
            this.setState({isEdit: false});
        }
    }
    componentWillUnmount() {
        SdnAccountStore.unlisten(this.updateState);
    }
    updateState = (state) => {
        this.setState(state);
    }
    render() {
        let html;
        let body;
        let title = "Launchpad: Add SDN Account";
        if (this.props.edit) {
            title = "Launchpad: Edit SDN Account";
        }
        if (this.props.isDashboard) {
            body = (<div>Edit or Create New Accounts</div>);
        } else {
             body = <SdnAccount {...this.props} store={SdnAccountStore} actions={SdnAccountActions} edit={this.state.isEdit} />
        }
        html = (<div className="cloud-account-wrapper">
                  <AppHeader title={title}  isLoading={this.state.isLoading} />
                    <div className="flex">
                      <AccountSidebar/>
                      {body}
                    </div>
              </div>);
        return html;
    }
}

LaunchpadSdnAccount.contextTypes = {
    router: React.PropTypes.object
};
