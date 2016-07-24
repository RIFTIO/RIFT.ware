/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import LoggingActions from '../logging/loggingActions.js';
import LoggingSource from '../logging/loggingSource.js';
import HeaderActions from './headerActions.js';
import Alt from '../skyquake_container/skyquakeAltInstance';

class HeaderStoreConstructor {
    constructor() {
        var self = this;
        this.validateErrorEvent = 0;
        this.validateErrorMsg = '';
        // this.exportAsync(LoggingSource);
        // this.bindActions(LoggingActions);
        this.bindActions(HeaderActions);
        this.exportPublicMethods({
            validateReset: self.validateReset
        })
    }
    getSysLogViewerURLError = () => {
        this.validateError("Log URL has not been configured.");
    }
    getSysLogViewerURLSuccess = (data) => {
        window.open(data.url);
    }
    showError = (msg) => {
        console.log('message received');
        this.setState({
            validateErrorEvent: true,
            validateErrorMsg: msg
        });
    }
    validateReset = () => {
        this.setState({
            validateErrorEvent: false
        });
    }
}

export default Alt.createStore(HeaderStoreConstructor)
