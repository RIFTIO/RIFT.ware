
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import TopologyL2Store from './topologyL2Store.js';
import TopologyL2Actions from './topologyL2Actions.js';
import RecordDetail from '../recordViewer/recordDetails.jsx';
import './topologyL2View.scss';
import TopologyDetail from './detailView.jsx';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import AppHeader from 'widgets/header/header.jsx';
import TopologyL2Graph from 'widgets/topology/topologyL2Graph.jsx';
import Button from 'widgets/button/rw.button.js';

export default class TopologyL2view extends React.Component {
    constructor(props) {
        super(props);
        this.state = TopologyL2Store.getState();
        TopologyL2Store.listen(this.storeListener);
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
    openDebug = () => {
        this.compoentWillUnmount();
        let loc = window.location.hash.split('/');
        loc.pop();
        loc.pop();
        loc.push('lp-debug');
        window.location.hash = loc.join('/');
    }
    storeListener = (state) => {
        this.setState(state);
    }

    componentWillUnmount() {
        TopologyL2Store.closeSocket();
        TopologyL2Store.unlisten(this.storeListener);
    }
    componentDidMount() {
        TopologyL2Store.getTopologyData('dummy-id');
    }

    onNodeEvent = (node_id) => {
        TopologyL2Actions.nodeClicked(node_id);
    }

    handleReloadData = () => {
        console.log("TopologyView.handleReloadData");
        this.componentDidMount();
    }

    render() {
        let html;
         let location = this.props.location;
              let navItems = [{
          name: 'Viewport',
          onClick: this.context.router.push.bind(this, {pathname:'/viewport', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
        },{
          name: 'COMPUTE TOPOLOGY',
          onClick: this.context.router.push.bind(this, {pathname:'/compute-topology', query: {id: location.query.id, sdnpresent: location.query.sdnpresent}})
        },{
             name: 'NETWORK TOPOLOGY'
        }
        ];

        let nav = <AppHeader title="Launchpad: Viewport: Network Topology" nav={navItems} />
        let reloadButton = null;
        if (this.state.ajax_mode) {
            reloadButton = <Button label="Reload data" className="reloadButton"
                onClick={this.handleReloadData} />
        }

        html = (
            <div className="app-body topologyL2ViewAppBody">
                {nav}
                {reloadButton}
                <div className="topologyL2View">
                    <i className="corner-accent top left"></i>
                    <i className="corner-accent top right"></i>
                    <TopologyL2Graph data={this.state.topologyData}
                                     nodeEvent={this.onNodeEvent}
                                     debugMode={this.props.debugMode}
                                     headerExtras={reloadButton}
                    />
                    <TopologyDetail data={this.state.detailData || {}}
                                    isLoading={this.state.isLoading}
                                    debugMode={this.props.debugMode}
                    />
                    <i className="corner-accent bottom left"></i>
                    <i className="corner-accent bottom right"></i>
                </div>
            </div>
        );

        return html;
    }
}

TopologyL2view.contextTypes = {
    router: React.PropTypes.object
};
TopologyL2view.propTypes = {
    debugMode: React.PropTypes.bool
}
TopologyL2view.defaultProps = {
    debugMode: false
}
