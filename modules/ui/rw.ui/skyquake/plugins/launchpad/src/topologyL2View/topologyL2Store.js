/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import TopologyL2Actions from './topologyL2Actions.js';
import TopologyL2Source from './topologyL2Source.js';
import Alt from '../alt';
let rw = require('utils/rw.js');
class TopologyL2Store {
    constructor() {
        var self = this;
        // initial state
        this.isLoading = true;
        this.topologyData = {
            nodes: [],
            links: [],
            network_ids: []
        };
        this.errorMessage = null;
        this.socket = null;
        this.detailView = null;

        this.bindActions(TopologyL2Actions);
        // bind source listeners
        this.exportAsync(TopologyL2Source);
        this.exportPublicMethods({
            closeSocket: this.closeSocket,
            getTopologyData: this.getTopologyData
        });
        this.ajax_mode = rw.getSearchParams(window.location).ajax_mode || false;
    }

    getTopologyData = (id) => {
        if (this.ajax_mode) {
            this.getInstance().fetchTopology();
        } else {
            this.getInstance().openTopologyApiSocket(id);
        }
    }
    openTopologyApiSocketLoading() {}
    openTopologyApiSocketSuccess(connection) {
        let self = this;
        let connectionManager = (type, connection) => {
            if (!connection) {
                console.warn('There was an issue connecting to the ' + type + ' socket');
                return;
            }
            if (self.socket) {
                self.socket.close();
                self.socket = null;
            }
            self.setState({
                socket: connection
            });
            connection.onmessage = function(data) {
                self.setState({
                    topologyData: JSON.parse(data.data),
                    isLoading: false
                });
            };
        }

        connectionManager('foo-type', connection);

    }
    openTopologyApiSocketError() {}

    handleLogout = () => {
        this.closeSocket();
    }

    closeSocket = () => {
        if (this.socket) {
            this.socket.close();
        }
        this.setState({
            socket: null
        });

        this.detailView = null;
        this.hasSelected = false;

        this.bindListeners({
            getTopologyApiSuccess: TopologyL2Actions.GET_TOPOLOGY_API_SUCCESS,
            getTopologyApiLoading: TopologyL2Actions.GET_TOPOLOGY_API_LOADING,
            getTopologyApiError: TopologyL2Actions.GET_TOPOLOGY_API_ERROR
        });
        // bind source listeners
        this.exportAsync(TopologyL2Source);
    }

    getTopologyApiSuccess = (data) => {
        this.setState({
            topologyData: data,
            errorMessage: null
        });
    }

    getTopologyApiLoading = () => {}

    getTopologyApiError = (errorMessage) => {
        this.errorMessage = errorMessage;
    }

    getNodeData = (node_id) => {
        // find node in thisa.topologyData.nodes
        var node_data = this.topologyData.nodes.find(
            function(element, index, array) {
                return (element.id == node_id);
            });
        return node_data;
    }

    nodeClicked = (node_id) => {
        this.setState({
            detailData: this.getNodeData(node_id)
        });
    }
}
    export default Alt.createStore(TopologyL2Store);
