
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import TopologyStore from './topologyStore.js';
import RecordDetail from '../recordViewer/recordDetails.jsx';
import './topologyView.scss';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import AppHeader from 'widgets/header/header.jsx';
import TopologyTree from 'widgets/topology/topologyTree.jsx';
import LaunchpadBreadcrumbs from '../launchpadBreadcrumbs.jsx';
import Button from 'widgets/button/rw.button.js';

export default class Topologyview extends React.Component {
    constructor(props) {
        super(props);
        this.state = TopologyStore.getState();
        TopologyStore.listen(this.storeListener);
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
        TopologyStore.closeSocket();
        TopologyStore.unlisten(this.storeListener);
    }
    componentDidMount() {
        let nsrRegEx = new RegExp("([0-9a-zA-Z-]+)\/compute-topology");
        let nsr_id;

        try {
            nsr_id =  this.props.location.query.id;
        } catch (e) {
            console.log("TopologyView.componentDidMount exception: ", e);
        }
        TopologyStore.getTopologyData(nsr_id);
    }
    selectNode = (node) => {
        TopologyStore.selectNode(node);
    }

    handleReloadData = () => {
        this.componentDidMount();
    }

    render() {
        let location = this.props.location;
        let html;
        let navItems = [{
          name: 'Viewport',
          onClick: this.context.router.push.bind(this, {pathname:'/viewport', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
        },{
          name: 'COMPUTE TOPOLOGY'
        }];


        if (location.query.sdnpresent == 'true') {
            navItems.push({
             name: 'NETWORK TOPOLOGY',
              onClick: this.context.router.push.bind(this, {pathname:'/network-topology', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
            });
        }
        let nav = <AppHeader nav={navItems} />
        html = (
            <div className="app-body topologyAppBody">
            {nav}
                <div className="topologyView">
                    <i className="corner-accent top left"></i>
                    <i className="corner-accent top right"></i>
                    <TopologyTree
                        data={this.state.topologyData}
                        selectNode={this.selectNode}
                        hasSelected={this.state.hasSelected}
                    />
                    <RecordDetail data={this.state.detailView || {}} isLoading={this.state.detailsLoading} />
                    <i className="corner-accent bottom left"></i>
                    <i className="corner-accent bottom right"></i>
                </div>
            </div>
        );
        return html;
    }
}
Topologyview.contextTypes = {
    router: React.PropTypes.object
};
