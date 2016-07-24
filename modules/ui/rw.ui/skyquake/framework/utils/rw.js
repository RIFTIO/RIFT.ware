
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

// rw.js will no longer be necessary when Angular dependency no longer exists
/**
 * reset values in an array, useful when an array instance is
 * being observed for changes and simply setting array reference
 * to a new array instance is not a great option.
 *
 * Example:
 *   x = ['a', 'b']
 *   setValues(x, ['c', 'd'])
 *   x
 *     ['c', 'd']
 */

var rw = rw || {
  // Angular specific for now but can be modified in one location if need ever be
  BaseController: function() {
    var self = this;

    // handles factories with detach/cancel methods and listeners with cancel method
    if (this.$scope) {
      this.$scope.$on('$stateChangeStart', function() {
        var properties = Object.getOwnPropertyNames(self);

        properties.forEach(function(key) {

          var propertyValue = self[key];

          if (propertyValue) {
            if (Array.isArray(propertyValue)) {
              propertyValue.forEach(function(item) {
                if (item.off && typeof item.off == 'function') {
                  item.off(null, null, self);
                }
              });
              propertyValue.length = 0;
            } else {
              if (propertyValue.detached && typeof propertyValue.detached == 'function') {
                propertyValue.detached();
              } else if (propertyValue.cancel && typeof propertyValue.cancel == 'function') {
                propertyValue.cancel();
              } else if (propertyValue.off && typeof propertyValue.off == 'function') {
                propertyValue.off(null, null, self);
              }
            }
          }
        });
      });
    };

    // call in to do additional cleanup
    if (self.doCleanup && typeof self.doCleanup == 'function') {
      self.doCleanup();
    };
  },
  getSearchParams: function (url) {
    var a = document.createElement('a');
    a.href = url;
    var params = {};
    var items = a.search.replace('?', '').split('&');
    for (var i = 0; i < items.length; i++) {
      if (items[i].length > 0) {
        var key_value = items[i].split('=');
        params[key_value[0]] = key_value[1];
      }
    }
    return params;
  },

  inplaceUpdate : function(ary, values) {
    var args = [0, ary.length];
    Array.prototype.splice.apply(ary, args.concat(values));
  }
};

// explore making this configurable somehow
//   api_server = 'http://localhost:5050';
rw.search_params = rw.getSearchParams(window.location.href);
// MONKEY PATCHING
if (Element.prototype == null) {
  Element.prototype.uniqueId = 0;
}

Element.prototype.generateUniqueId = function() {
  Element.prototype.uniqueId++;
  return 'uid' + Element.prototype.uniqueId;
};

Element.prototype.empty = Element.prototype.empty || function() {
  while(this.firstChild) {
    this.removeChild(this.firstChild);
  }
};

/**
 * Merge one object into another.  No circular reference checking so if there
 * is there might be infinite recursion.
 */
rw.merge = function(obj1, obj2) {
  for (prop in obj2) {
    if (typeof(obj2[prop]) == 'object') {
      if (prop in obj1) {
        this.merge(obj1[prop], obj2[prop]);
      } else {
        obj1[prop] = obj2[prop];
      }
    } else {
      obj1[prop] = obj2[prop];
    }
  }
}

Element.prototype.getElementByTagName = function(tagName) {
  for (var i = this.children.length - 1; i >= 0; i--) {
    if (this.children[i].localName == tagName) {
      return this.children[i];
    }
  }
};

rw.ui = {

  computedWidth : function(elem, defaultValue) {
    var s = window.getComputedStyle(elem);
    var w = s['width'];
    if (w && w != 'auto') {
      // I've never seen this case, but here anyway
      return w;
    }
    w = s['min-width'];
    if (w) {
      return w;
    }
    return defaultValue;
  },

  computedHeight : function(elem, defaultValue) {
    var s = window.getComputedStyle(elem);
    var w = s['height'];
    if (w && w != 'auto') {
      // I've never seen this case, but here anyway
      return w;
    }
    w = s['min-height'];
    if (w) {
      return w;
    }
    return defaultValue;
  },

  computedStyle : function(elem, property, defaultValue) {
    var s = window.getComputedStyle(elem);
    if (s[property]) {
      return s[property];
    }
    return defaultValue;
  },

  odd : function(n) {
    return Math.abs(n) % 2 == 1 ? 'odd' : '';
  },

  status : function(s) {
    return s == 'OK' ? 'yes' : 'no';
  },

  capitalize: function(s) {
    return s ? s.charAt(0).toUpperCase() + s.slice(1) : '';
  },

  fmt: function(n, fmtStr) {
    return numeral(n).format(fmtStr);
  },

  // assumes values are in megabytes!
  bytes: function(n, capacity) {
    if (n === undefined || isNaN(n)) {
      return '';
    }
    var units = false;
    if (capacity === undefined) {
      capacity = n;
      units = true;
    }
    var suffixes = [
      ['KB' , 1000],
      ['MB' , 1000000],
      ['GB' , 1000000000],
      ['TB' , 1000000000000],
      ['PB' , 1000000000000000]
    ];
    for (var i = 0; i < suffixes.length; i++) {
      if (capacity < suffixes[i][1]) {
        return (numeral((n * 1000) / suffixes[i][1]).format('0,0') + (units ? suffixes[i][0] : ''));
      }
    }
    return n + (units ? 'B' : '');
  },

  // assumes values are already in megabits!
  bits: function(n, capacity) {
    if (n === undefined || isNaN(n)) {
      return '';
    }
    var units = false;
    if (capacity === undefined) {
      capacity = n;
      units = true;
    }
    var suffixes = [
      ['Mbps' , 1000],
      ['Gbps' , 1000000],
      ['Tbps' , 1000000000],
      ['Pbps' , 1000000000000]
    ];
    for (var i = 0; i < suffixes.length; i++) {
      if (capacity < suffixes[i][1]) {
        return (numeral((n  * 1000) / suffixes[i][1]).format('0,0') + (units ? suffixes[i][0] : ''));
      }
    }
    return n + (units ? 'Bps' : '');
  },

  ppsUtilization: function(pps) {
    return pps ? numeral(pps / 1000000).format('0.0') : '';
  },
  ppsUtilizationMax: function(item) {
    var rate = item.rate / 10000;
    var max = item.max * 0.0015;
    return rate/max;
  },
  bpsAsPps: function(speed) {
    return parseInt(speed) * 0.0015;
  },

  upperCase: function(s) {
    return s.toUpperCase()
  },

  mbpsAsPps: function(mbps) {
    var n = parseInt(mbps);
    return isNaN(n) ? 0 : rw.ui.fmt(rw.ui.bpsAsPps(n * 1000000), '0a').toUpperCase();
  },

  k: function(n) {
    return rw.ui.fmt(rw.ui.noNaN(n), '0a');
  },

  noNaN: function(n) {
    return isNaN(n) ? 0 : n;
  },

  // Labels used in system
  l10n : {
    vnf: {
      'trafsimclient': 'Traf Sim Client',
      'trafsimserver': 'Traf Sim Server',
      'ltemmesim': 'MME',
      'ltegwsim': 'SAE Gateway',
      'trafgen': 'Traf Gen Client',
      'trafsink': 'Traf Gen Server',
      'loadbal': 'Load Balancer',
      'slbalancer': 'Scriptable Load Balancer'
    }
  }
};

rw.math = {
  editXml : function(xmlTemplate, domEditor) {
    var str2dom = new DOMParser();
    var dom = str2dom.parseFromString(xmlTemplate, 'text/xml');
    if (domEditor) {
      domEditor(dom);
    }
    var dom2str = new XMLSerializer();
    return dom2str.serializeToString(dom);
  },

  num : function(el, tag) {
    return parseInt(this.str(el, tag), 10);
  },

  str : function(el, tag) {
    var tags = el.getElementsByTagName(tag);
    return tags.length > 0 ? tags[0].textContent.trim() : '';
  },

  sum : function(total, i, key, value) {
    if (_.isNumber(value)) {
      total[key] = (i === 0 ? value : (total[key] + value));
    }
  },

  sum2 : function(key) {
    return function(prev, cur, i) {
      var value = cur[key];
      if (_.isNumber(value)) {
        if (typeof(prev) === 'undefined') {
          return value;
        } else {
          return prev + value;
        }
      }
      return prev;
    };
  },

  max : function(key) {
    return function(prev, cur, i) {
      var value = cur[key];
      if (_.isNumber(value)) {
        if (typeof(prev) === 'undefined') {
          return value;
        } else if (prev < value) {
          return value;
        }
      }
      return prev;
    };
  },

  avg2 : function(key) {
    var sum = rw.math.sum2(key);
    return function(prev, cur, i, ary) {
      var s = sum(prev, cur, i);
      if (i === ary.length - 1) {
        return s / ary.length;
      }
      return s;
    };
  },

  avg : function(rows, key) {
    var total = XmlMath.total(rows, key);
    return total / rows.length;
  },

  total : function(rows, key) {
    var total = 0;
    for (var i = rows.length - 1; i >= 0; i--) {
      var n =  parseInt(rows[i][key]);
      if (!isNaN(n)) {
        total += n;
      }
    }
    return total;
  },

  run : function(total, rows, operation) {
    var i;
    var f = function(value, key) {
      operation(total, i, key, value);
    };
    for (i = 0; i < rows.length; i++) {
      _.each(rows[i], f);
    }
  }
};


rw.db = {
  open: function (name, onInit, onOpen) {
    var self = this;

    var open = window.indexedDB.open(name, 2);

    open.onerror = function (e) {
      console.log('Could not open database', name, e.target.error.message);
    };

    open.onsuccess = function (e) {
      var db = e.target.result;
      onOpen(db);
    };

    open.onupgradeneeded = function (e) {
      var db = e.target.result;
      onInit(db);
    };
  }
};

rw.db.Offline = function(name) {
  this.name = name;
  this.datastore = 'offline';
};

rw.db.Offline.prototype = {

  open : function(onOpen) {
    rw.db.open(this.name, this.init.bind(this), onOpen);
  },

  getItem : function(url, onData) {
    var self = this;
    this.open(function(db) {
      var query = db.transaction(self.datastore)
          .objectStore(self.datastore)
          .get(url);
      query.onsuccess = function(e) {
        if (e.target.result) {
          onData(e.target.result.data);
        } else {
          console.log('No data found for ' + url + '.  You may need to rebuild your offline database');
        }
      }
    });
  },

  init : function(db) {
    var self = this;
    if (!db.objectStoreNames.contains(this.datastore)) {
      var create = db.createObjectStore(this.datastore, {keyPath: 'url'});
      create.onerror = function(e) {
        console.log('Could not create object store ' + this.datastore);
      }
    }
  },

  saveStore : function(store) {
    var self = this;
    this.open(function(db) {
      for (var i = 0; i < store.length; i++) {
        var save = db.transaction(self.datastore, "readwrite")
            .objectStore(self.datastore)
            .put(store[i]);
      }
    });
  }
};

rw.api = {
  nRequests : 0,
  tRequestsMs: 0,
  tRequestMaxMs: 0,
  nRequestsByUrl: {},
  statsId: null,

  resetStats: function() {
    rw.api.nRequests = 0;
    rw.api.tRequestsMs = 0;
    rw.api.tRequestMaxMs = 0;
    rw.api.nRequestsByUrl = {};
  },

  handleAjaxError : function (req, status, err) {
    console.log('failed', req, status, err);
  },

  json: function(url) {
    return this.get(url, 'application/json')
  },

  get: function(url, accept) {
    var deferred = jQuery.Deferred();
    if (rw.api.offline) {
      rw.api.offline.getItem(url, function (data) {
        deferred.resolve(data);
      });
    } else {
      var startTime = new Date().getTime();
      jQuery.ajax(rw.api.server + url, {
        type: 'GET',
        dataType: 'json',
        error: rw.api.handleAjaxError,
        headers: rw.api.headers(accept),
        success: function (data) {
          rw.api.recordUrl(url, startTime);
          deferred.resolve(data);
        },
        error: function(e) {
          deferred.reject(e);
        }
      });
    }
    return deferred.promise();
  },

  headers: function(accept, contentType) {
    var h = {
      Accept: accept
    };
    if (rw.api.statsId != null) {
      h['x-stats'] = rw.api.statsId;
    }
    if (contentType) {
      h['Content-Type'] = contentType;
    }
    return h;
  },

  recordUrl:function(url, startTime) {
    var elapsed = new Date().getTime() - startTime;
    rw.api.tRequestsMs += elapsed;
    rw.api.nRequests += 1;
    rw.api.tRequestMaxMs = Math.max(rw.api.tRequestMaxMs, elapsed);
    if (url in rw.api.nRequestsByUrl) {
      var metric = rw.api.nRequestsByUrl[url];
      metric.url = url;
      metric.n += 1;
      metric.max = Math.max(metric.max, elapsed);
    } else {
      rw.api.nRequestsByUrl[url] = {url: url, n: 1, max: elapsed};
    }
  },

  put: function(url, data, contentType) {
    return this.push('PUT', url, data, contentType);
  },

  post: function(url, data, contentType) {
    return this.push('POST', url, data, contentType);
  },

  rpc: function(url, data, error) {
    if(error === undefined){
        error = function(a,b,c){
      }
    }
    return this.push('POST', url, data, 'application/vnd.yang.data+json', error);
  },

  push: function(method, url, data, contentType, errorFn) {
    var deferred = jQuery.Deferred();
    if (rw.api.offline) {
      // eating offline put request
      if(contentType == 'application/vnd.yang.data+json'){
        var payload = data;
        rw.api.offline.getItem(url, function (data) {
          deferred.resolve(data);
        });
      }
      deferred.resolve({});
    } else {
      var startTime = new Date().getTime();
      jQuery.ajax(rw.api.server + url, {
        type: method,
        error: rw.api.handleAjaxError,
        dataType: 'json',
        headers: rw.api.headers('application/json', contentType),
        data: JSON.stringify(data),
        success: function (response) {
          rw.api.recordUrl(url, startTime);
          deferred.resolve(response);
        },
        error: errorFn
      });
    }
    return deferred.promise();
  },

  setOffline: function(name) {
    if (name) {
      rw.api.offline = new rw.db.Offline(name);
    } else {
      rw.api.offline = false;
    }
  },

  // When passing things to ConfD ('/api/...') then '/' needs to be
  // %252F(browser) --> %2F(Rest) --> / ConfD
  encodeUrlParam: function(value) {
    var once = rw.api.singleEncodeUrlParam(value);
    return once.replace(/%/g, '%25');
  },

  // UrlParam cannot have '/' and encoding it using %2F gets double-encoded in flask
  singleEncodeUrlParam: function(value) {
    return value.replace(/\//g, '%2F');
  }
};

rw.api.SocketSubscriber = function(url) {
  this.url = url;

  this.id = ++rw.api.SocketSubscriber.uniqueId;

  this.subscribed = false;
  this.offlineRateMs = 2000;
},

rw.api.SocketSubscriber.uniqueId = 0;

rw.api.SocketSubscriber.prototype = {

  // does not support PUT/PORT with payloads requests yet
  websubscribe : function(webUrl, onload, offline) {
    this.subscribeMeta(onload, {
      url: webUrl
    }, offline);
  },

  subscribeMeta : function(onload, meta, offline) {
    var self = this;

    if (this.subscribed) {
      this.unsubscribe();
    }

    var m = meta || {};
    if (rw.api.offline) {
      this.subscribeOffline(onload, m, offline);
    } else {
      this.subscribeOnline(onload, m);
    }
  },

  subscribeOffline: function(onload, meta, offline) {
    var self = this;
    rw.api.json(meta.url).done(function(data) {
      var _update = function() {
        if (offline) {
          offline(data);
        } else {
          onload(data);
        }
      };

      this.offlineTimer = setInterval(_update, self.offlineRateMs);
    });
  },

  subscribeOnline: function(onload, meta) {
    var self = this;
    var _subscribe = function() {
      meta.widgetId = self.id;
      meta.accept = meta.accept || 'application/json';
      document.websocket().emit(self.url, meta);
      self.subscribed = true;
    };

    var _register = function() {
      document.websocket().on(self.url + '/' + self.id, function(dataString) {
        var data = dataString;

        // auto convert to object to make backward compatible
        if (meta.accept.match(/[+\/]json$/) && dataString != '') {
          data = JSON.parse(dataString);
        }
        onload(data);
      });
      _subscribe();
    };
    document.websocket().on('error',function(d){
      console.log('socket error', d)
    });
    document.websocket().on('close',function(d){
      console.log('socket close', d)
    })
    document.websocket().on('connect', _register);

    // it's possible this call is not nec. and will always be called
    // as part of connect statement above
    _register();
  },

  unsubscribe : function() {
    if (rw.api.offline) {
      if (this.offlineTimer) {
        clearInterval(this.offlineTimer);
        this.offlineTimer = null;
      }
    } else {
      var unsubscribe = { widgetId: this.id, enable: false };
      document.websocket().emit(this.url, unsubscribe);
      this.subscribed = false;
    }
  }
};

rw.api.server = rw.search_params['api_server'] || '';

document.websocket = function() {
  if ( ! this.socket ) {
    //io.reconnection = true;
    var wsServer = rw.api.server || 'http://' + document.domain + ':' + location.port;
    var wsUrl = wsServer + '/rwapp';
    this.socket = io.connect(wsUrl, {reconnection:true});
  }
  return this.socket;
};

rw.api.setOffline(rw.search_params['offline']);

rw.vnf = {
  ports: function(service) {
    return _.flatten(jsonPath.eval(service, '$.connector[*].interface[*].port'));
  },
  fabricPorts: function(service) {
    return _.flatten(jsonPath.eval(service, '$.vm[*].fabric.port'));
  }
};

rw.VcsVisitor = function(enter, leave) {
  this.enter = enter;
  this.leave = leave;
}

rw.VcsVisitor.prototype = {

  visit: function(node, parent, i, listType) {
    var hasChildren = this.enter(parent, node, i, listType);
    if (hasChildren) {
      switch (rw.vcs.nodeType(node)) {
        case 'rwsector':
          this.visitChildren(node, node.collection, 'collection');
          break;
        case 'rwcolony':
          this.visitChildren(node, node.collection, 'collection');
          this.visitChildren(node, node.vm, 'vm');
          break;
        case 'rwcluster':
          this.visitChildren(node, node.vm, 'vm');
          break;
        case 'RWVM':
          this.visitChildren(node, node.process, 'process');
          break;
        case 'RWPROC':
          this.visitChildren(node, node.tasklet, 'tasklet');
          break;
      }
    }
    if (this.leave) {
      this.leave(parent, node, obj, i, listType);
    }
  },

  visitChildren : function(parent, children, listType) {
    if (!children) {
      return;
    }
    var i = 0;
    var self = this;
    _.each(children, function(child) {
      self.visit.call(self, child, parent, i, listType);
      i += 1;
    });
  }
};

rw.vcs = {

  allVms : function() {
    return _.flatten([this.jpath('$.collection[*].vm'), this.jpath('$.collection[*].collection[*].vm')], true);
  },

  vms: function(n) {
    if (n == undefined || n === null || n === this) {
      return this.allVms();
    }
    switch (rw.vcs.nodeType(n)) {
      case 'rwcolony':
        return this.jpath('$.collection[*].vm[*]', n);
      case 'rwcluster':
        return this.jpath('$.vm[*]', n);
      case 'RWVM':
        return [n];
      default:
        return null;
    }
  },

  nodeType: function(node) {
    if (node.component_type === 'RWCOLLECTION') {
      return node.collection_info['collection-type'];
    }
    return node.component_type;
  },

  allClusters : function() {
    return this.jpath('$.collection[*].collection');
  },

  allColonies: function() {
    return this.jpath('$.collection');
  },

  allPorts:function(n) {
    switch (rw.vcs.nodeType(n)) {
      case 'rwsector':
        return this.jpath('$.collection[*].collection[*].vm[*].port[*]', n);
      case 'rwcolony':
        return this.jpath('$.collection[*].vm[*].port[*]', n);
      case 'rwcluster':
        return this.jpath('$.vm[*].port[*]', n);
      case 'RWVM':
        return this.jpath('$.port[*]', n);
      default:
        return null;
    }
  },

  allFabricPorts:function(n) {
    switch (rw.vcs.nodeType(n)) {
      case 'rwsector':
        return this.jpath('$.collection[*].collection[*].vm[*].fabric.port[*]', n);
      case 'rwcolony':
        return this.jpath('$.collection[*].vm[*].fabric.port[*]', n);
      case 'rwcluster':
        return this.jpath('$.vm[*].fabric.port[*]', n);
      case 'RWVM':
        return this.jpath('$.fabric.port[*]', n);
      default:
        return null;
    }
  },

  getChildren: function(n) {
    switch (rw.vcs.nodeType(n)) {
      case 'rwcolony':
        return 'vm' in n ? _.union(n.collection, n.vm) : n.collection;
      case 'rwcluster':
        return n.vm;
      case 'RWVM':
        return n.process;
      case 'RWPROC':
        return n.tasklet;
    }
    return [];
  },

  jpath : function(jpath, n) {
    return _.flatten(jsonPath.eval(n || this, jpath), true);
  }
};

rw.trafgen = {
  startedActual : null, // true or false once server-side state is loaded
  startedPerceived : false,
  ratePerceived : 25,
  rateActual : null, // non-null once server-side state is loaded
  packetSizePerceived : 1024,
  packetSizeActual: null
};

rw.trafsim = {
  startedActual : null, // true or false once server-side state is loaded
  startedPerceived : false,
  ratePerceived : 50000,
  rateActual : null, // non-null once server-side state is loaded
  maxRate: 200000
};

rw.aggregateControlPanel = null;

rw.theme = {
  // purple-2 and blue-2
  txBps : 'hsla(212, 57%, 50%, 1)',
  txBpsTranslucent : 'hsla(212, 57%, 50%, 0.7)',
  rxBps : 'hsla(212, 57%, 50%, 1)',
  rxBpsTranslucent : 'hsla(212, 57%, 50%, 0.7)',
  txPps : 'hsla(260, 35%, 50%, 1)',
  txPpsTranslucent : 'hsla(260, 35%, 50%, 0.7)',
  rxPps : 'hsla(260, 35%, 50%, 1)',
  rxPpsTranslucent : 'hsla(260, 35%, 50%, 0.7)',
  memory : 'hsla(27, 98%, 57%, 1)',
  memoryTranslucent : 'hsla(27, 98%, 57%, 0.7)',
  cpu : 'hsla(123, 45%, 50%, 1)',
  cpuTranslucent : 'hsla(123, 45%, 50%, 0.7)',
  storage : 'hsla(180, 78%, 25%, 1)',
  storageTranslucent : 'hsla(180, 78%, 25%, 0.7)'
};

rw.RateTimer = function(onChange, onStop) {
  this.rate = 0;
  this.onChange = onChange;
  this.onStop = onStop;
  this.testFrequency = 500;
  this.testWaveLength = 5000;
  this.timer = null;
  return this;
};

rw.RateTimer.prototype = {
  start: function() {
    this.rate = 0;
    if (!this.timer) {
      this.n = 0;
      var strategy = this.smoothRateStrategy.bind(this);
      this.testingStartTime = new Date().getTime();
      this.timer = setInterval(strategy, this.testFrequency);
    }
  },

  stop: function() {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
      this.onStop();
    }
  },

  smoothRateStrategy: function() {
    var x = (new Date().getTime() - this.testingStartTime) / this.testWaveLength;
    this.rate = Math.round(100 * 0.5 * (1 - Math.cos(x)));
    // in theory you could use wavelength and frequency to determine stop but this
    // is guaranteed to stop at zero.
    this.onChange(this.rate);
    this.n += 1;
    if (this.n >= 10 && this.rate < 1) {
      this.stop();
    }
  }
};

module.exports = rw;
