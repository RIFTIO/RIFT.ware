/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
//Login needs to be refactored. Too many cross dependencies
var AuthActions = require('../widgets/login/loginAuthActions.js');
var $ = require('jquery');
var Utils = {};

var INACTIVITY_TIMEOUT = 600000;


Utils.getInactivityTimeout = function() {
    return new Promise(function(resolve, reject) {
        $.ajax({
            url: '//' + window.location.hostname + ':3000/inactivity-timeout',
            type: 'GET',
            success: function(data) {
                resolve(data);
            },
            error: function(error) {
                console.log("There was an error getting the inactivity-timeout: ", error);
                reject(error);
            }
        }).fail(function(xhr) {
            console.log('There was an xhr error getting the inactivity-timeout', xhr);
            reject(xhr);
        });
    });

};

Utils.bootstrapApplication = function() {
    var self = this;
    return new Promise(function(resolve, reject) {
        Promise.all([self.getInactivityTimeout()]).then(function(results) {
            INACTIVITY_TIMEOUT = results[0]['inactivity-timeout'];
            resolve();
        }, function(error) {
            console.log("Error bootstrapping application ", error);
            reject();
        });
    });
};

Utils.addAuthorizationStub = function(xhr) {
    var Auth = window.sessionStorage.getItem("auth");
    xhr.setRequestHeader('Authorization', 'Basic ' + Auth);
};

Utils.getByteDataWithUnitPrefix = function(number, precision) {
    var toPrecision = precision || 3;
    if (number < Math.pow(10, 3)) {
        return [number, 'B'];
    } else if (number < Math.pow(10, 6)) {
        return [(number / Math.pow(10, 3)).toPrecision(toPrecision), 'KB'];
    } else if (number < Math.pow(10, 9)) {
        return [(number / Math.pow(10, 6)).toPrecision(toPrecision), 'MB'];
    } else if (number < Math.pow(10, 12)) {
        return [(number / Math.pow(10, 9)).toPrecision(toPrecision), 'GB'];
    } else if (number < Math.pow(10, 15)) {
        return [(number / Math.pow(10, 12)).toPrecision(toPrecision), 'TB'];
    } else if (number < Math.pow(10, 18)) {
        return [(number / Math.pow(10, 15)).toPrecision(toPrecision), 'PB'];
    } else if (number < Math.pow(10, 21)) {
        return [(number / Math.pow(10, 18)).toPrecision(toPrecision), 'EB'];
    } else if (number < Math.pow(10, 24)) {
        return [(number / Math.pow(10, 21)).toPrecision(toPrecision), 'ZB'];
    } else if (number < Math.pow(10, 27)) {
        return [(number / Math.pow(10, 24)).toPrecision(toPrecision), 'ZB'];
    } else {
        return [(number / Math.pow(10, 27)).toPrecision(toPrecision), 'YB'];
    }
}

Utils.getPacketDataWithUnitPrefix = function(number, precision) {
    var toPrecision = precision || 3;
    if (number < Math.pow(10, 3)) {
        return [number, 'P'];
    } else if (number < Math.pow(10, 6)) {
        return [(number / Math.pow(10, 3)).toPrecision(toPrecision), 'KP'];
    } else if (number < Math.pow(10, 9)) {
        return [(number / Math.pow(10, 6)).toPrecision(toPrecision), 'MP'];
    } else if (number < Math.pow(10, 12)) {
        return [(number / Math.pow(10, 9)).toPrecision(toPrecision), 'GP'];
    } else if (number < Math.pow(10, 15)) {
        return [(number / Math.pow(10, 12)).toPrecision(toPrecision), 'TP'];
    } else if (number < Math.pow(10, 18)) {
        return [(number / Math.pow(10, 15)).toPrecision(toPrecision), 'PP'];
    } else if (number < Math.pow(10, 21)) {
        return [(number / Math.pow(10, 18)).toPrecision(toPrecision), 'EP'];
    } else if (number < Math.pow(10, 24)) {
        return [(number / Math.pow(10, 21)).toPrecision(toPrecision), 'ZP'];
    } else if (number < Math.pow(10, 27)) {
        return [(number / Math.pow(10, 24)).toPrecision(toPrecision), 'ZP'];
    } else {
        return [(number / Math.pow(10, 27)).toPrecision(toPrecision), 'YP'];
    }
}
Utils.loginHash = "#/login";
Utils.setAuthentication = function(username, password, cb) {
    var self = this;
    var AuthBase64 = btoa(username + ":" + password);
    window.sessionStorage.setItem("auth", AuthBase64);
    self.detectInactivity();
    if (cb) {
        cb();
    }
}
Utils.clearAuthentication = function(callback) {
    var self = this;
    window.sessionStorage.removeItem("auth");
    AuthActions.notAuthenticated();
    window.sessionStorage.setItem("locationRefHash", window.location.hash);
    if (callback) {
        callback();
    } else {
        window.location.hash = Utils.loginHash;
    }
}
Utils.isNotAuthenticated = function(windowLocation, callback) {
    var self = this;
    self.detectInactivity();
    if (!window.sessionStorage.getItem("auth")) {
        Utils.clearAuthentication();
    }
}
Utils.isDetecting = false;
Utils.detectInactivity = function(callback, duration) {
    var self = this;
    if (!self.isDetecting) {
        var cb = function() {
            self.clearAuthentication();
            if (callback) {
                callback();
            }
        };
        var isInactive;
        var timeout = duration || INACTIVITY_TIMEOUT;
        var setInactive = function() {
            isInactive = setTimeout(cb, timeout);
        };
        var reset = function() {
            clearTimeout(isInactive);
            setInactive();
        }
        setInactive();
        window.addEventListener('mousemove', reset);
        window.addEventListener("keypress", reset);
        self.isDetecting = true;
    }
}
Utils.checkAuthentication = function(statusCode, cb) {
    var self = this;
    if (statusCode == 401) {
        if (cb) {
            cb();
        }
        window.sessionStorage.removeItem("auth")
        self.isNotAuthenticated(window.location)
        return true;
    }
    return false;
}
Utils.getHostNameFromURL = function(url) {
    var match = url.match(/^(https?\:)\/\/(([^:\/?#]*)(?:\:([0-9]+))?)([^?#]*)(\?[^#]*|)(#.*|)$/);
    return match && match[3];
}

Utils.webSocketProtocol = function() {
    if (this.wsProto) {
        return this.wsProto;
    } else {
        if (window.location.protocol == 'http:') {
            this.wsProto = 'ws:'
        } else {
            this.wsProto = 'wss:'
        }
    }
    return this.wsProto;
}

Utils.arrayIntersperse = (arr, sep) => {
    if (arr.length === 0) {
        return [];
    }

    return arr.slice(1).reduce((xs, x, i) => {
        return xs.concat([sep, x]);
    }, [arr[0]]);
}

module.exports = Utils;
