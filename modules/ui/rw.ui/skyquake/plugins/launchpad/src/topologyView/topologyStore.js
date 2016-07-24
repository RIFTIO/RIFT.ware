/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import TopologyActions from './topologyActions.js';
import TopologySource from './topologySource.js';
// import source
import Alt from '../alt';
let rw = require('utils/rw.js');
class TopologyStore {
    constructor() {
        var self = this;
        // initial state
        this.isLoading = true;
        this.topologyData = {};
        this.socket = null;
        this.detailView = null;
        this.hasSelected = false;
        // bind action listeners
        this.bindActions(TopologyActions);

        // bind source listeners
        this.exportAsync(TopologySource);
        this.exportPublicMethods({
            selectNode: this.selectNode,
            closeSocket: this.closeSocket,
            getTopologyData: this.getTopologyData
        })
        this.ajax_mode = rw.getSearchParams(window.location).ajax_mode || false;
    }
    selectNode = (node) => {
        var apiType = {
            'nsr' : 'getRawNSR',
            'vdur' : 'getRawVDUR',
            'vnfr': 'getRawVNFR'
        }
        // TODO: VISIT
       apiType[node.type] && this.getInstance()[apiType[node.type]](node.id, node.parent ? node.parent.id : undefined);
    }
    getRawSuccess = (data) => {
        this.setState({
            detailView: data
        });
    }
    getRawLoading = () => {

    }
    getRawError = () => {

    }

    getTopologyData = (nsr_id) => {
        if (this.ajax_mode) {
            this.getInstance().getNSRTopology(nsr_id);
        } else {
            this.getInstance().openNSRTopologySocket(nsr_id);
        }
    }

    openNSRTopologySocketLoading = () => {
         console.log('loading')
    }
    openNSRTopologySocketSuccess = (connection) => {
        let self = this;
        console.log('success');
        let connectionManager = (type, connection) => {
            if (!connection) {
                console.warn('There was an issue connecting to the ' + type + ' socket');
                return;
            }
            if (self.socket) {
                self.socket.close();
                self.socket = null;
            }
            connection.onmessage = function(data) {
                var tData = JSON.parse(data.data);
                var newState = {
                    topologyData: tData,
                    isLoading: false,
                    socket: connection
                };
                if(!self.hasSelected) {
                    newState.hasSelected = true;
                    self.selectNode(tData);
                }
                self.setState(newState);

            };
        }

        connectionManager('nsr', connection);
    }
    openNSRTopologySocketError = () => {
        console.log('error')
    }
    handleLogout = () => {
        this.closeSocket();
    }
    closeSocket = () => {
        if (this.socket) {
            this.socket.close();
        }
        this.setState({
            socket: null
        })
    }
    getNSRTopologySuccess = (data) => {
        this.setState({
            topologyData: data,
            errorMessage: null,
            isLoading: false
        });
    }
    getNSRTopologyLoading = () => {}
    getNSRTopologyError = (errorMessage) => {
        console.log('error', errorMessage)
        //this.errorMessage = errorMessage;
    }

}
export default Alt.createStore(TopologyStore);
