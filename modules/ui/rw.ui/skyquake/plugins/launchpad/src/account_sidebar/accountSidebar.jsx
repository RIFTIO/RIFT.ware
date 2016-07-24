/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import './accountSidebar.scss';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import CloudAccountStore from '../launchpad_cloud_account/cloudAccountStore.js';
import ConfigAgentAccountStore from '../launchpad_config_agent_account/configAgentAccountStore';
import SdnAccountStore from '../launchpad_sdn_account/sdnAccountStore';
import { Link } from 'react-router';
export default class AccountSidebar extends React.Component{
    constructor(props) {
        super(props);
        var self = this;
        this.state = {}
        this.state.cloudAccounts = CloudAccountStore.getState().cloudAccounts || [];
        CloudAccountStore.listen(self.updateCloudState);
        CloudAccountStore.getCloudAccounts();

        this.state.configAgentAccounts = ConfigAgentAccountStore.getState().configAgentAccounts || [];
        ConfigAgentAccountStore.listen(this.updateConfigAgentState);
        ConfigAgentAccountStore.getConfigAgentAccounts();

        this.state.sdnAccounts = SdnAccountStore.getState().sdnAccounts || [];
        SdnAccountStore.listen(this.updateSdnState);
        SdnAccountStore.getSdnAccounts();
    }
    updateCloudState = (state) => {
        this.setState({
                cloudAccounts: state.cloudAccounts
        });
    }
    updateConfigAgentState = (state) => {
        this.setState({
                configAgentAccounts: state.configAgentAccounts
        });
    }
    updateSdnState = (state) => {
        this.setState({
                sdnAccounts: state.sdnAccounts
        });
    }
    componentWillUnmount() {
        CloudAccountStore.unlisten(this.updateCloudState);
        ConfigAgentAccountStore.unlisten(this.updateConfigAgentState);
        SdnAccountStore.unlisten(this.updateSdnState);
    }
    render() {
        let html;
        let cloudAccounts = (this.state.cloudAccounts.length > 0) ? this.state.cloudAccounts.map(function(account, index) {
            return (
                <DashboardCard  key={index} className='pool-card accountSidebarCard'>
                <header>
                <Link to={'cloud-accounts/' + account.name} title="Edit Account">
                        <h3>{account.name}</h3>
                </Link>
                </header>
                    {
                        account.pools.map(function(pool, i) {
                        // return <a className={pool.type + ' link-item'} key={i} href={'#/pool/' + account.name +'/' + pool.name}>{pool.name}</a>
                        })
                    }
                    <a className="empty-pool link-item"  href={"#/pool/" + account.name + '/' } style={{cursor: 'pointer', display: 'none'}}>
                        <span>
                            <h2 className="create-title">Create Pool</h2>
                        </span>
                        <img src={require("style/img/launchpad-add-fleet-icon.png")}/>
                    </a>
                </DashboardCard>
            )
        }) : null;
        let sdnAccounts = (this.state.sdnAccounts && this.state.sdnAccounts.length > 0) ? this.state.sdnAccounts.map(function(account, index) {
            return (
                <DashboardCard key={index} className='pool-card accountSidebarCard'>
                     <header>
                        <Link to={'sdn-accounts/' + account.name} title="Edit Account">
                        <h3>{account.name}</h3>
                </Link>
                    </header>
                </DashboardCard>
            )
        }) : null;
        let configAgentAccounts = (this.state.configAgentAccounts.length > 0) ? this.state.configAgentAccounts.map(function(account, index) {
            return (
                <DashboardCard key={index} className='pool-card accountSidebarCard'>
                <header>
                    <Link to={'config-agent-accounts/' + account.name} title="Edit Account">
                        <h3>{account.name}</h3>
                </Link>
                </header>
                </DashboardCard>
            )
        }) : null;
        html = (
            <div className='accountSidebar'>
                <h1>Cloud Accounts</h1>
                {cloudAccounts}
                <DashboardCard className="accountSidebarCard">
                        <Link
                        to={{pathname: 'cloud-accounts/create'}}
                        title="Create Cloud Account"
                        className={'accountSidebarCard_create'}
                    >
                            Add Cloud Account
                            <img src={require("style/img/launchpad-add-fleet-icon.png")}/>
                        </Link>
                </DashboardCard>
                <h1>SDN Accounts</h1>
                {sdnAccounts}
                <DashboardCard className="accountSidebarCard">
                        <Link
                        to={{pathname: 'sdn-accounts/create'}}
                        title="Create Sdn Account"
                        className={'accountSidebarCard_create'}
                    >
                            Add SDN Account
                            <img src={require("style/img/launchpad-add-fleet-icon.png")}/>
                        </Link>
                </DashboardCard>
                <h1>Config Agent Accounts</h1>
                {configAgentAccounts}
                <DashboardCard className="accountSidebarCard">
                    <Link
                        to={{pathname: 'config-agent-accounts/create'}}
                        title="Create Config Agent Account"
                        className={'accountSidebarCard_create'}
                    >
                            Add Config Agent Account
                            <img src={require("style/img/launchpad-add-fleet-icon.png")}/>
                        </Link>
                </DashboardCard>
            </div>
                );
        return html;
    }
}

AccountSidebar.defaultProps = {
    cloudAccounts: [],
    sdnAccounts: [],
    configAgentAccounts: []
}
