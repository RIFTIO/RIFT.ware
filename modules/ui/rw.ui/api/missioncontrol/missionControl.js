/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var request = require('request');
var Promise = require('promise');
var utils = require('../utils/utils.js');
var constants = require('../common/constants.js');
var _ = require('underscore');


var Cloud = {}; // Cloud-Account API object
var mgmtDomain = {};
var vmPool = {};
var networkPool = {};
var mc = {};
var pool = {};
var Sdn = {};
var crashDetails = {};
var pools = {};
var general = {}



general.uptime = function(req) {
  var api_server = req.query["api_server"];
  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/mission-control/uptime',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        if (utils.validateResponse('general.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body);
          } catch (e) {
            return reject({});
          }
          return resolve(data)

        }
      })
  })
}

general.createTime = function(req) {
  var api_server = req.query["api_server"];
  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/mission-control/create-time',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        if (utils.validateResponse('general.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body);
          } catch (e) {
            return reject({});
          }
          return resolve(data)

        }
      })
  })
}


// Management Domain APIs
mgmtDomain.get = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id) {
    // Get all mgmt-domains
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.collection, {
          'Authorization': req.get('Authorization')
        });
      request({
          url: utils.confdPort(api_server) + '/api/operational/mgmt-domain/domain?deep',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;

          if (utils.validateResponse('mgmtDomain.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body).collection['rw-mc:domain'];
            } catch (e) {
              return reject({});
            }

            return resolve(data);
          }
        });
    });
  } else {
    // Get specific mgmt-domain
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.data, {
          'Authorization': req.get('Authorization')
        });
      request({
          url: utils.confdPort(api_server) + '/api/operational/mgmt-domain/domain/' + id,
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;

          if (utils.validateResponse('mgmtDomain.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body)['rw-mc:domain'];
            } catch (e) {
              console.log('Problem with "mgmtDomain.get for id="', id, e);
              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "mgmtDomain.get": ' + e.toString()
              }
              return reject(err);
            }
            return resolve(data);
          }
        });
    });
  }

};
/**
 * @param  {String} api_server Location of confd
 * @param  {Object} data Object containing new mgmt-domain data.
 * @return {Promise}
 */
mgmtDomain.create = function(req) {

  var api_server = req.query["api_server"];
  var data = req.body;

  // Validate data
  if (!data.name) {
    return new Promise(function(resolve, reject) {
      console.log('Bad params creating mgmt-domain');
      return reject({
        statusCode: 500,
        errorMessage: {
          error: 'Bad params creating mgmt-domain. Missing name.'
        }
      });
    });
  }

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "domain": [{
        "name": data.name,
        "pools": {
          "vm": [],
          "network": []
        }
      }]
    };

    if (data.networkPool) {
      jsonData['domain'][0]['pools']['network'].push({
        'name': data.networkPool
      });
    };
    if (data.vmPool) {
      jsonData['domain'][0]['pools']['vm'].push({
        'name': data.vmPool
      });
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/mgmt-domain',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('mgmtDomain.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    })
  });
};

mgmtDomain.delete = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id || !api_server) {
    return new Promise(function(resolve, reject) {
      console.log('Must specifiy api_server and id to delete mgmt-domain');
      return reject({
        statusCode: 500,
        errorMessage: {
          error: 'Must specifiy api_server and id to delete mgmt-domain'
        }
      });
    });
  };

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
      url: utils.confdPort(api_server) + '/api/config/mgmt-domain/domain/' + id,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false
    }, function(error, response, body) {
      if (utils.validateResponse('mgmtDomain.delete', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    });

  });
};

/**
 * @param  {String} api_server Location of confd
 * @param  {Object} data Object containing new mgmt-domain data.
 * @return {Promise}
 */
mgmtDomain.update = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;
  var data = req.body;

  // Validate data
  if (!id || !data.name) {
    return new Promise(function(resolve, reject) {
      console.log('Bad params updating mgmt-domain');
      return reject({
        statusCode: 500,
        errorMessage: {
          error: 'Bad params updating mgmt-domain. id and name must be specified'
        }
      });
    });
  };

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-mc:domain": {
        "name": data.name,
        "pools": {
          "vm": [],
          "network": []
        }
      }
    };

    if (data.networkPool) {
      jsonData['rw-mc:domain']['pools']['network'].push({
        'name': data.networkPool
      });
    };
    if (data.vmPool) {
      jsonData['rw-mc:domain']['pools']['vm'].push({
        'name': data.vmPool
      });
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/mgmt-domain/domain/' + id,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('mgmtDomain.update', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    })
  });
};

// MC starts the launchpad associated with the mgmt domain.
mgmtDomain.startLaunchpad = function(req) {

  var api_server = req.query["api_server"];
  var name = req.params.name;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "input": {
        "mgmt-domain": name
      }
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/operations/start-launchpad',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('mgmtDomain.startLaunchpad', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    })
  });
};

// MC stops the launchpad associated with the mgmt domain.
mgmtDomain.stopLaunchpad = function(req) {

  var api_server = req.query["api_server"];
  var name = req.params.name;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "input": {
        "mgmt-domain": name
      }
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/operations/stop-launchpad',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('mgmtDomain.stopLaunchpad', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    })
  });
};

// Cloud-Account APIs
Cloud.get = function(req) {
  var self = this;

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id) {
    // Get all cloud accounts
    return new Promise(function(resolve, reject) {

      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.collection, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/cloud/account',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Cloud.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body).collection['rw-mc:account']
            } catch (e) {
              console.log('Problem with "Cloud.get"', e);
              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Cloud.get": ' + e.toString()
              }
              return reject(err);
            }

            return resolve(self.poolAggregate(data));
          };
        });
    });
  } else {
    //Get a specific cloud account
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.data, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + id,
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Cloud.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body)['rw-mc:account'];
            } catch (e) {
              console.log('Problem with "Cloud.get"', e);
              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Cloud.get": ' + e.toString()
              }
              return reject(err);
            }

            return resolve(data);
          }
        });
    });
  }
};

Cloud.create = function(req) {

  var api_server = req.query["api_server"];
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "account": Array.isArray(data) ? data : [data]
    };

    console.log('Creating with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/cloud',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    });
  });
};

Cloud.update = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-mc:account": data
    };

    console.log('Updating ', id, ' with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/cloud/account/' + id,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.update', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    });
  });
};

Cloud.delete = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id || !api_server) {
    return new Promise(function(resolve, reject) {
      console.log('Must specifiy api_server and id to delete cloud account');
      return reject({
        statusCode: 500,
        errorMessage: {
          error: 'Must specifiy api_server and id to delete cloud account'
        }
      });
    });
  };

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    request({
      url: utils.confdPort(api_server) + '/api/config/cloud/account/' + id,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false
    }, function(error, response, body) {
      if (utils.validateResponse('Cloud.delete', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      };
    });
  });
};

Cloud.getResources = function(req) {

  var api_server = req.query["api_server"];
  var cloudAccount = req.query["cloud_account"];

  return new Promise(function(resolve, reject) {

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });

    request({
        url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + cloudAccount + '/resources?deep',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;
        if (utils.validateResponse('Cloud.getResources', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rw-mc:resources']
          } catch (e) {
            console.log('Problem with "Cloud.getResources"', e);

            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "Cloud.getResources": ' + e.toString()
            }

            return reject(err);
          }

          return resolve(data);
        };
      });
  });
};

Cloud.getPools = function(req) {

  var api_server = req.query["api_server"];
  var cloudAccount = req.query["cloud-account"];

  return new Promise(function(resolve, reject) {

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });

    request({
        url: utils.confdPort(api_server) + '/api/operational/cloud/account/' + cloudAccount + '/pools',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;
        if (utils.validateResponse('Cloud.getPools', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body)['rw-mc:pools']
          } catch (e) {
            console.log('Problem with "Cloud.getPools"', e);
            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "Cloud.getPools": ' + e.toString()
            }

            return reject(err);
          }

          return resolve(data);
        }
      });
  });
}

Cloud.poolAggregate = function(cloudAccounts) {
  cloudAccounts.forEach(function(ca) {
    var oldPools = ca.pools;
    var newPools = [];
    for (type in oldPools) {
      oldPools[type].forEach(function(pool) {
        pool.type = type;
        newPools.push(pool);
      })
    }
    ca.pools = newPools;
  });
  return cloudAccounts;
}

//Sdn Account APIs
Sdn.get = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  var self = this;
  if (!id) {
    // Get all sdn accounts
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.collection, {
          'Authorization': req.get('Authorization')
        });
      request({
          url: utils.confdPort(api_server) + '/api/operational/sdn/account',
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Cloud.delete', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body).collection['rw-mc:account']
            } catch (e) {
              console.log('Problem with "Sdn.get"', e);

              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Sdn.get": ' + e.toString()
              }

              return reject(err);
            }

            return resolve(data);
          }
        });
    });
  } else {
    //Get a specific sdn account
    return new Promise(function(resolve, reject) {
      var requestHeaders = {};
      _.extend(requestHeaders,
        constants.HTTP_HEADERS.accept.data, {
          'Authorization': req.get('Authorization')
        });

      request({
          url: utils.confdPort(api_server) + '/api/operational/sdn/account/' + id,
          type: 'GET',
          headers: requestHeaders,
          forever: constants.FOREVER_ON,
          rejectUnauthorized: false
        },
        function(error, response, body) {
          var data;
          if (utils.validateResponse('Sdn.get', error, response, body, resolve, reject)) {
            try {
              data = JSON.parse(response.body)['rw-mc:account'];
            } catch (e) {
              console.log('Problem with "Sdn.get"', e);

              var err = {};
              err.statusCode = 500;
              err.errorMessage = {
                error: 'Problem with "Sdn.get": ' + e.toString()
              }

              return reject(err);
            }

            return resolve(data);
          }
        });
    });
  }
};

Sdn.create = function(req) {

  var api_server = req.query["api_server"];
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "account": Array.isArray(data) ? data : [data]
    };

    console.log('Creating with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/sdn',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    });
  });
};

Sdn.update = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;
  var data = req.body;

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-mc:account": data
    };

    console.log('Updating ', id, ' with', JSON.stringify(jsonData));

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/sdn/account/' + id,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.update', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    });
  });
};

Sdn.delete = function(req) {

  var api_server = req.query["api_server"];
  var id = req.params.id;

  if (!id || !api_server) {
    return new Promise(function(resolve, reject) {
      console.log('Must specifiy api_server and id to delete sdn account');
      reject({
        statusCode: 500,
        errorMessage: {
          error: 'Must specifiy api_server and id to delete sdn account'
        }
      });
    });
  };

  return new Promise(function(resolve, reject) {

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/sdn/account/' + id,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false
    }, function(error, response, body) {
      if (utils.validateResponse('Sdn.delete', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    });
  });
};

/**
 * Get all VM pools
 * @param {String} api_server - The api server to hit
 * @return {Array} vm-pools
 */
vmPool.get = function(req) {

  var api_server = req.query["api_server"];

  var self = this;
  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.collection, {
        'Authorization': req.get('Authorization')
      });

    request({
        url: utils.confdPort(api_server) + '/api/operational/vm-pool/pool',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;

        if (utils.validateResponse('Sdn.get', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body).collection['rw-mc:pool']
          } catch (e) {
            console.log('Problem with "vmPool.get"', e);

            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "vmPool.get"' + e.toString()
            }

            return reject(err);
          }

          return resolve(data);
        }
      });
  });
};


vmPool.create = function(req) {

  var api_server = req.query["api_server"];
  var data = {
    name: req.body.name,
    'cloud-account': req.body['cloud-account'],
    assigned: req.body.assigned,
    'dynamic-scaling': req.body['dynamic-scaling']
  };

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "pool": Array.isArray(data) ? data : [data]
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/vm-pool',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('vmPool.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    })
  });
};

vmPool.delete = function(req) {

  var api_server = req.query["api_server"];
  var name = req.body.name;

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
      });
    console.log(name)
    console.log(utils.confdPort(api_server) + '/api/running/vm-pool/pool/' + name)
    request({
      url: utils.confdPort(api_server) + '/api/config/vm-pool/pool/' + name,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
    }, function(error, response, body) {
      if (utils.validateResponse('vmPool.delete', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    })
  });
};

vmPool.edit = function(req) {

  var api_server = req.query["api_server"];
  var data = {
    name: req.body.name,
    "cloud-account": req.body["cloud-account"],
    assigned: req.body.assigned,
    "dynamic-scaling": req.body['dynamic-scaling']
  };

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-mc:pool": Array.isArray(data) ? data : [data]
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/vm-pool/pool/' + data.name,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('vmPool.edit', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    })
  });
};

networkPool.create = function(req) {

  var api_server = req.query["api_server"];
  var data = {
    name: req.body.name,
    'cloud-account': req.body['cloud-account'],
    assigned: req.body.assigned,
    'dynamic-scaling': req.body['dynamic-scaling']
  };

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "pool": Array.isArray(data) ? data : [data]
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/network-pool',
      method: 'POST',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('networkPool.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    });
  });
};

networkPool.delete = function(req) {

  var api_server = req.query["api_server"];
  var name = req.body.name;

  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });
    request({
      url: utils.confdPort(api_server) + '/api/config/network-pool/pool/' + name,
      method: 'DELETE',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
    }, function(error, response, body) {
      if (utils.validateResponse('networkPool.create', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    });
  });
};

networkPool.edit = function(req) {

  var api_server = req.query["api_server"];
  var data = {
    name: req.body.name,
    "cloud-account": req.body["cloud-account"],
    assigned: req.body.assigned,
    "dynamic-scaling": req.body['dynamic-scaling']
  };

  return new Promise(function(resolve, reject) {
    var jsonData = {
      "rw-mc:pool": Array.isArray(data) ? data : [data]
    };

    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.data,
      constants.HTTP_HEADERS.content_type.data, {
        'Authorization': req.get('Authorization')
      });

    request({
      url: utils.confdPort(api_server) + '/api/config/network-pool/pool/' + data.name,
      method: 'PUT',
      headers: requestHeaders,
      forever: constants.FOREVER_ON,
      rejectUnauthorized: false,
      json: jsonData,
    }, function(error, response, body) {
      if (utils.validateResponse('networkPool.edit', error, response, body, resolve, reject)) {
        return resolve(JSON.stringify(response.body));
      }
    })
  });
};


/**
 * Get all network pools
 * @param {String} api_server - The api server to hit
 * @return {Array} network-pools
 */
networkPool.get = function(req) {

  var api_server = req.query["api_server"];

  var self = this;
  return new Promise(function(resolve, reject) {
    var requestHeaders = {};
    _.extend(requestHeaders,
      constants.HTTP_HEADERS.accept.collection, {
        'Authorization': req.get('Authorization')
      });
    request({
        url: utils.confdPort(api_server) + '/api/operational/network-pool/pool',
        type: 'GET',
        headers: requestHeaders,
        forever: constants.FOREVER_ON,
        rejectUnauthorized: false
      },
      function(error, response, body) {
        var data;
        if (utils.validateResponse('Sdn.delete', error, response, body, resolve, reject)) {
          try {
            data = JSON.parse(response.body).collection['rw-mc:pool']
          } catch (e) {
            console.log('Problem with "networkPool.get"', e);

            var err = {};
            err.statusCode = 500;
            err.errorMessage = {
              error: 'Problem with "networkPool.get": ' + e.toString()
            }

            return reject(err);
          }

          return resolve(data);
        }
      });
  });
};



/**
 * Get aggregated pool info for create-management-domain page
 * @param {String} api_server - The api server to hit
 */
pools.get = function(req) {

  var api_server = req.query["api_server"];

  return new Promise(function(resolve, reject) {
    Promise.all([
        vmPool.get(req),
        networkPool.get(req)
      ])
      .then(function(results) {
        var poolsObject = {};
        poolsObject.vmPools = results[0];
        poolsObject.networkPools = results[1];
        resolve(poolsObject);
      }, function(error) {
        console.log('error getting pools', error);
      });
  });
};



mc["cloud-accounts"] = Cloud;
mc["sdn-accounts"] = Sdn;
mc["mgmt-domain"] = mgmtDomain;
mc["vm-pools"] = vmPool;
mc["network-pools"] = networkPool;
mc["pools"] = pools;
mc["general"] = general;

module.exports = mc;