
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
function storageAvailable(type) {
	try {
		var storage = window[type];
		var x = '__storage_test_DEADBEEF__';
		storage.setItem(x, x);
		storage.removeItem(x);
		return true;
	} catch (e) {
		return false;
	}
}

export class LaunchpadSettings {

  constructor(args) {
    this.openedNsrsStorageKey = "openedNsrList";
    this.debug = (args && args.debug);

    if (!storageAvailable('localStorage')) {
    	this.canUseStorage = false;
    	console.log("Unable to use window.localStorage");
    	//throw "Unable to use window.localStorage";

    } else {
    	this.canUseStorage = true;
    }
  }	
  openedNSRs() {
  	if(this.debug) {
  	  console.log("LaunchpadSettings.openedNSRs called");	
  	}
  	var nsrs = null;
  	if (this.canUseStorage) {
  		let nsrStr = window.localStorage.getItem(this.openedNsrsStorageKey);
  		if (nsrStr) {
  			nsrs = nsrStr.split(',');
  		}
  		if (this.debug) {
  		  console.log("got nsr ids=", nsrs);
  		}
  	}
    return (nsrs) ? nsrs : [];
  }

  addOpenNSR(nsrId) {
  	let nsrs = this.openedNSRs();
  	if (this.debug) {
  	  console.log("LaunchpadSettings.addOpenNSR. nsrs=", nsrs);	
  	}
  	
  	if (!nsrs.includes(nsrId)) {
  		if (this.debug) {
  		  console.log("adding nsr id=", nsrId);	
  		}
  		nsrs.unshift(nsrId);
  		window.localStorage.setItem(this.openedNsrsStorageKey, nsrs);
  	}
  	return this;

  }

  removeOpenNSR(nsrId) {
  	if (this.debug) {
  	  console.log("LaunchpadSettings.removeOpenNSR called with id:", nsrId);	
  	}
  	if (this.canUseStorage) {
  		let nsrs = this.openedNSRs().filter(nsr_id => nsr_id != nsrId);
  		window.localStorage.setItem(this.openedNsrsStorageKey, nsrs);
  	}
  	return this;
  }
	
  removeAllOpenNSRs() {
  	if (this.canUseStorage) {
  		window.localStorage.removeItem(this.openedNsrsStorageKey);
  	}
  }
}

/*
module.exports = {
	LaunchpadSettings: LaunchpadSettings
}
*/