
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from './alt';

var FleetSource = require('./launchpadFleetSource.js');
var FleetActions = require('./launchpadFleetActions.js');
import CardActions from './launchpad_card/launchpadCardActions.js';
var Utils = require('utils/utils.js');
import LaunchNetworkServiceSource from './network_service_launcher/launchNetworkServiceSource.js';
import LaunchNetworkServiceActions from './network_service_launcher/launchNetworkServiceActions.js';
import LoggingSource from 'widgets/logging/loggingSource.js';

import {LaunchpadSettings} from './settings.js';


var FleetStore;
var _ = require('underscore');
//  _.debounce(function(){});
function FleetStoreConstructor() {
  var self = this;
  this.fleets = [];
  this.descriptorCount = 0;
  this.socket = null;
  this.selectedSlaParam = '';
  this.launchpads = [];
  this.nsrs = [];
  this.isStandAlone = false;
  this.registerAsync(FleetSource);
  this.exportAsync(LoggingSource);
  this.exportAsync(LaunchNetworkServiceSource);
  this.slideno = 0;
  this.dropdownSlide = ['', 0];
  this.slideChange = -1;
  this.validateErrorEvent = 0;
  this.launchpadSettings = new LaunchpadSettings();
  this.openedNsrIDs = this.launchpadSettings.openedNSRs();
  this.isNsListPanelVisible = true;
  this.bindListeners({
    //NEW
    //Socket Actions
    openNSRSocketLoading: FleetActions.openNSRSocketLoading,
    openNSRSocketSuccess: FleetActions.openNSRSocketSuccess,
    //Card Actions
    handleUpdateControlInput: CardActions.updateControlInput,
    //Source Actions
    handleNsrControlSuccess: FleetActions.nsrControlSuccess,
    handleNsrControlError: FleetActions.nsrControlError,
    handleSlideNoStateChange: FleetActions.slideNoStateChange,
    handleSlideNoStateChangeSuccess: FleetActions.slideNoStateChangeSuccess,
    getNsrInstancesSuccess: FleetActions.getNsrInstancesSuccess,
    getNsrInstancesError: FleetActions.getNsrInstancesError,
    deleteNsrInstanceSuccess: FleetActions.deleteNsrInstanceSuccess,
    deletingNSR: FleetActions.deletingNSR,
    deleteNsrInstanceError: FleetActions.deleteNsrInstanceError,
    getLaunchpadConfigSuccess: [FleetActions.getLaunchpadConfigSuccess,
      LaunchNetworkServiceActions.getLaunchpadConfigSuccess],
    setNSRStatusSuccess: FleetActions.setNSRStatusSuccess,
    setNSRStatusError: FleetActions.setNSRStatusError,
    validateReset: FleetActions.validateReset,
    validateError: FleetActions.validateError,
    //Launch Network Service Source Actions
    getCatalogSuccess: LaunchNetworkServiceActions.getCatalogSuccess,
    // Card management actions
    openNsrCard: FleetActions.openNsrCard,
    closeNsrCard: FleetActions.closeNsrCard,
    instantiateNetworkService: FleetActions.instantiateNetworkService,
    setNsListPanelVisible: FleetActions.setNsListPanelVisible,

  });
  this.bindAction(LaunchNetworkServiceActions.launchNSRSuccess, responseData => {
    try {
      this.openNsrCard(responseData.data.nsr_id);
    } catch (e) {
      console.log("Unable to open NS Card for response data: ", responseData);
    }
  });
  this.exportPublicMethods({
    getFleets: function() {
      return this.getState().fleets;
    },
    closeSocket: this.closeSocket.bind(self)
  });
}

FleetStoreConstructor.prototype.handleLogout = function() {
  this.closeSocket();
}

FleetStoreConstructor.prototype.closeSocket = function() {
  if(this.socket) {
    this.socket.close();
  }
  this.setState({
    socket:null
  })
}

FleetStoreConstructor.prototype.getSysLogViewerURLError = function(data) {
  if (data.requester == 'lp') {

    this.validateError("Log URL has not been configured.");
  }
};

//NEW
FleetStoreConstructor.prototype.openNSRSocketLoading = function(connection) {
  console.log('open socketloading')
  Alt.actions.global.showScreenLoader.defer();
};
FleetStoreConstructor.prototype.getLaunchpadConfigSuccess = function(config) {
  var isStandAlone = ((!config) || config["operational-mode"] == "STANDALONE");
  this.setState({
    isStandAlone: isStandAlone
  });
};
FleetStoreConstructor.prototype.openNSRSocketSuccess = function(connection) {
  var self = this;
  var isLoading = true;
  if (!connection) return;
  self.setState({
    socket: connection
  });
  connection.onmessage = function(socket) {
    try {
      var data = JSON.parse(socket.data);
      if (!data.nsrs) {
        console.warn('NSRS property not present on the payload, check that the api server is functioning correct and that the LP is fully launched. Received: ', data);
        data.nsrs = [];
      }
      Utils.checkAuthentication(data.statusCode, function() {
        self.closeSocket();
      });
      let deletingNSRs = [];

      if (self.nsrs) {
        deletingNSRs = _.filter(self.nsrs, function(nsr) {
          return nsr.deleting == true;
        });
      };

      deletingNSRs.forEach(function(deletingNSR) {
        data.nsrs.map(nsr => {
          if (nsr.id == deletingNSR.id) {
            _.extend(nsr, deletingNSR);
          }
        });
      });
      if(isLoading) {
        isLoading = false;
        Alt.actions.global.hideScreenLoader.defer();
      }
      self.setState({
        nsrs: data.nsrs
      });
     } catch(e) {

    }
  };
}
FleetStoreConstructor.prototype.getSysLogViewerURLSuccess = function(data) {
  if (data.requester == 'lp') {
    window.open(data.url);
  }
};
FleetStoreConstructor.prototype.getNsrInstancesSuccess = function(data) {
  this.setState({
    nsrs: data.nsrs
  });
};
FleetStoreConstructor.prototype.deleteNsrInstanceSuccess = function(data) {
  console.log('deleted', data)
};

FleetStoreConstructor.prototype.deletingNSR = function(id) {
  console.log('deleting NSR', id);
  let nsrs = [];
  let self = this;
  try {
    nsrs = this.nsrs.map(nsr => {
      if (nsr.id == id) {
        nsr.deleting = true;
        self.closedNsrCard(id);
      }
      return nsr;
    });
    this.setState({
      nsrs: nsrs
    })
  } catch (e) {
    console.log('No NSR\'s found. Should never get here');
  }
};

FleetStoreConstructor.prototype.deleteNsrInstanceError = function(data) {};
FleetStoreConstructor.prototype.getNsrInstancesError = function(data) {
  console.log('ERROR', data)
};
FleetStoreConstructor.prototype.handleUpdateControlInput = _.debounce(function(data) {
  var opt = data[0];
  FleetStore.nsrControl(opt.operation, opt.url, data[1])
}, 500).bind(null);
FleetStoreConstructor.prototype.handleNsrControlSuccess = function(data) {
  console.log(data)
};
FleetStoreConstructor.prototype.handleNsrControlError = function() {};
FleetStoreConstructor.prototype.handleSlideNoStateChange = function(data) {
  this.setState({
    dropdownSlide: data.pane,
    slideno: data.no,
    slideChange: data.slideChange
  });
};
FleetStoreConstructor.prototype.handleSlideNoStateChangeSuccess = function() {
  this.setState({
    slideChange: this.slideChange - 1
  });
};
FleetStoreConstructor.prototype.setNSRStatusSuccess = function() {};
FleetStoreConstructor.prototype.setNSRStatusError = function(data) {
  console.log('Error changing NSR State', data)
};

FleetStoreConstructor.prototype.getCatalogSuccess = function(data) {
  var self = this;
  var descriptorCount = 0;
  data.forEach(function(catalog) {
    descriptorCount += catalog.descriptors.length;
  });

  self.setState({
    descriptorCount: descriptorCount
  });
};

FleetStoreConstructor.prototype.validateError = function(msg) {
  this.setState({
    validateErrorEvent: true,
    validateErrorMsg: msg
  });
};
FleetStoreConstructor.prototype.validateReset = function() {
  this.setState({
    validateErrorEvent: false
  });
};

// Card management
FleetStoreConstructor.prototype.openNsrCard = function(id) {
  //console.log("*** *** FleetStore.openNsrCard with nsr id:", id);
  const openedNsrIDs = this.openedNsrIDs.slice(0);
  // Only add if card is not there
  if (id) {
    if (!openedNsrIDs.includes(id)) {
      openedNsrIDs.unshift(id);
      this.launchpadSettings.addOpenNSR(id);
      this.setState({
        openedNsrIDs: openedNsrIDs
      });
    } else {
      console.log("NSR already open, id:%s", id);
    }
  } else {
    console.log("null nsr id.");
  }
}
FleetStoreConstructor.prototype.closeNsrCard = function(id) {
  this.launchpadSettings.removeOpenNSR(id);
  this.setState({
      openedNsrIDs: this.openedNsrIDs.filter(nsr_id => nsr_id !== id)
  });
}
FleetStoreConstructor.prototype.instantiateNetworkService = function(id) {
  window.location.hash = window.location.hash + '/launch';
}

FleetStoreConstructor.prototype.setNsListPanelVisible = function(isVisible) {
  this.setState({
    isNsListPanelVisible: isVisible
  })
}

FleetStore = Alt.createStore(FleetStoreConstructor);
module.exports = FleetStore;
