/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var cors = require('cors');
var fs = require('fs');
var missionControlAPI = require('./missioncontrol/missionControl.js');
var launchpadAPI = require('./launchpad/launchpad.js');
var aboutAPI = require('./about/about.js')
var debugAPI = require('./debug/debug.js')
var cloudAPI = require('./cloud_account/cloudAccount.js')
var sdnAPI = require('./sdn_account/sdnAccount.js')
var bodyParser = require('body-parser');
var utils = require('./utils/utils.js');
var logging = require('./logging/logging.js');
// var nock = require('nock');
// nock.recorder.rec({
//   dont_print: true
// });
// var fixtures = nock.recorder.play();
function sendErrorResponse(error, res) {
    res.status(error.statusCode);
    if (error.statusCode == 401) {
        res.append('WWW-Authenticate', 'Basic');
    }
    res.send(error);
}

function sendSuccessResponse(response, res) {
    res.status(response.statusCode);
    res.send(response.data);
}
var routes = function(app, socketManager) {
    app.use(bodyParser.json());
    app.use(cors());
    app.use(bodyParser.urlencoded({
        extended: true
    }));

    // Begin auth related routes
    function checkAuthorization(req, res, next) {
        utils.checkAuthorizationHeader(req).then(
            //success
            function() {
                next();
            },
            //error
            function() {
                res.status(401);
                res.send({
                    error: 'Authentication information required'
                });
            });
    };

    app.all('/launchpad*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });

    app.all('/mission-control*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });

    app.all('/logging*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });

    app.all('/socket-polling*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });

    app.all('/api*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });

    app.all('/cloud-account*', cors(), function(req, res, next) {
        checkAuthorization(req, res, next);
    });
    // End auth related routes

    // Test route
    app.get('/test', cors(), function(req, res) {
        res.statusCode = 200;
        res.send({
            hello: 'world'
        });
    });
    // Test end

    // Begin unauntheticated APIs

    // Used by QA
    app.get('/inactivity-timeout', cors(), function(req, res) {
        var response = {
            statusCode: 200,
            data: {
                'inactivity-timeout': process.env.UI_TIMEOUT_SECS || 600000
            }
        }
        sendSuccessResponse(response, res);
    });

    // End unauntheticated APIs

    // Begin launchpad page APIs
    //
    app.get('/launchpad/nsr', cors(), function(req, res) {
        launchpadAPI['nsr'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.get('/launchpad/nsr/:id', cors(), function(req, res) {
        launchpadAPI['nsr'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.delete('/launchpad/nsr/:id', cors(), function(req, res) {
        launchpadAPI['nsr'].delete(req).then(function(response) {
            sendSuccessResponse(response, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/launchpad/nsr', cors(), function(req, res) {
        launchpadAPI['nsr'].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            console.log(error)
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.get('/launchpad/nsr/:nsr_id/vnfr', cors(), function(req, res) {
        launchpadAPI['vnfr'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        }).catch(function(error) {
            console.log(error)
        });
    });
    app.put('/launchpad/nsr/:id/admin-status', cors(), function(req, res) {
        launchpadAPI['nsr'].setStatus(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.post('/launchpad/nsr/:id/:scaling_group_id/instance', cors(), function(req, res) {
        launchpadAPI['nsr'].createScalingGroupInstance(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            console.log(error)
            sendErrorResponse(error, res);
        });
    });
    app.delete('/launchpad/nsr/:id/:scaling_group_id/instance/:scaling_instance_id', cors(), function(req, res) {
        launchpadAPI['nsr'].deleteScalingGroupInstance(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            console.log(error)
            sendErrorResponse(error, res);
        });
    });
    app.get('/launchpad/vnfr', cors(), function(req, res) {
        launchpadAPI['vnfr'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.get('/launchpad/vnfr/:id', cors(), function(req, res) {
        launchpadAPI['vnfr'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.get('/launchpad/vnfr/:vnfr_id/:vdur_id', cors(), function(req, res) {
        launchpadAPI['vdur'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    app.get('/launchpad/catalog', cors(), function(req, res) {
        launchpadAPI['catalog'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            if(!error || !error.statusCode) {
                error = {
                    statusCode: 500,
                    message: 'unknown error with Catalog.get'
                }
            }
            sendErrorResponse(error, res);
        });
    });
    //TODO refactor this query
    app.get('/launchpad/decorated-catalog', cors(), function(req, res) {
        launchpadAPI['catalog'].get(req).then(function(data) {
            var returnData = decorateNsdCatalogWithPlacementGroups(data)
            sendSuccessResponse(returnData, res);
        }, function(error) {
            if(!error || !error.statusCode) {
                error = {
                    statusCode: 500,
                    message: 'unknown error with Catalog.get'
                }
            }
            sendErrorResponse(error, res);
        });
    });
function decorateNsdCatalogWithPlacementGroups(catalog) {
    var newData = catalog;
    var parsedCatalog = JSON.parse(catalog.data);
    var nsds = parsedCatalog[0].descriptors;
    var vnfds = parsedCatalog[1].descriptors;
    var vnfdDict = (function(){
        var dict = {};
        vnfds.map(function(v, i) {
            dict[v.id] = v;
        })
        return dict;
    })(vnfds);

    nsds.map(function(c, i) {
        //Rename and decorate NSD placement groups
        c['ns-placement-groups'] = c['placement-groups'] && c['placement-groups'].map(function(p, i) {
            //Adds vnfd name to member-vnfd entry
            p['member-vnfd'] = p['member-vnfd'].map(function(v) {
                v.name = vnfdDict[v['vnfd-id-ref']].name;
                return v;
            });
            p['host-aggregate'] = [];
            return p;
        });
        //Remove 'placement-group' entry
        delete c['placement-groups'];

        //Adds vnf placement groups to nsd object for UI
        c['vnf-placement-groups'] = [];
        c['constituent-vnfd'] && c['constituent-vnfd'].map(function(v) {
            var vnf = vnfdDict[v['vnfd-id-ref']];
            // var vnfPg = {
            //     name: vnf.name,
            //     'placement-groups': vnf['placement-groups'].map(function(vp){
            //         vp['host-aggregate'] = [{}];
            //         return vp;
            //     })
            // };
            v['vnf-name'] = vnf.name;
            vnf['placement-groups'] && vnf['placement-groups'].map(function(vp) {
                vp['host-aggregate'] = [];
                vp['vnf-name'] = vnf.name;
                vp['vnfd-id-ref'] = v['vnfd-id-ref'];
                vp['member-vnf-index'] = v['member-vnf-index'];
                c['vnf-placement-groups'].push(vp);
            })
        })
        return c;
    })
    // parsedCatalog[0].descriptors = nsds;
    newData.data = JSON.stringify(parsedCatalog);
    return newData;
}

    //TODO refactor this query
    app.post('/launchpad/vnfd', cors(), function(req, res) {
        launchpadAPI['catalog'].getVNFD(req).then(function(data) {
            res.send(data);
        }).catch(function(error) {
            console.log(error)
        });
    });
    app.get('/launchpad/nsd/:nsd_id/input-param', cors(), function(req, res) {
        launchpadAPI['nsd'].getInputParams(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/launchpad/exec-ns-config-primitive', cors(), function(req, res) {
        launchpadAPI['rpc'].executeNSConfigPrimitive(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/launchpad/get-ns-config-primitives', cors(), function(req, res) {
        launchpadAPI['rpc'].executeNSConfigPrimitive(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/launchpad/catalog/:catalogType/:id', cors(), function(req, res) {
        launchpadAPI['catalog'].delete(req).then(function(response) {
            res.status(response.statusCode);
            res.send({});
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.post('/launchpad/catalog/:catalogType', cors(), function(req, res) {
        launchpadAPI['catalog'].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.put('/launchpad/catalog/:catalogType/:id', cors(), function(req, res) {
        launchpadAPI['catalog'].update(req).then(function(data) {
            res.send(data);
        }, function(error) {
            res.status(error.statusCode);
            res.send(error.errorMessage);
        });
    });
    app.get('/launchpad/nsr/:id/compute-topology', cors(), function(req, res) {
        launchpadAPI['computeTopology'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    app.get('/launchpad/network-topology', cors(), function(req, res) {
        launchpadAPI['networkTopology'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    app.get('/launchpad/cloud-account', cors(), function(req, res) {
        launchpadAPI['cloud_account'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    app.get('/launchpad/config', cors(), function(req, res) {
        launchpadAPI['config'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    app.get('/launchpad/config-agent-account', cors(), function(req, res) {
        launchpadAPI['config-agent-account'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/launchpad/config-agent-account/:id', cors(), function(req, res) {
        launchpadAPI['config-agent-account'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/launchpad/config-agent-account', cors(), function(req, res) {
        launchpadAPI['config-agent-account'].create(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.put('/launchpad/config-agent-account/:id', cors(), function(req, res) {
        launchpadAPI['config-agent-account'].update(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/launchpad/config-agent-account/:id', cors(), function(req, res) {
        launchpadAPI['config-agent-account'].delete(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //DataCenters
    app.get('/launchpad/data-centers', cors(), function(req, res) {
        launchpadAPI['data_centers'].get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    // End launchpad page APIs

    // Begin mission control page APIs
    app.get('/mission-control/uptime', cors(), function(req, res) {
        missionControlAPI["general"].uptime(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });

    app.get('/mission-control/create-time', cors(), function(req, res) {
        missionControlAPI["general"].createTime(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/mgmt-domain', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].get(req).then(function(data) {
            if (!data || data.constructor.name != "Array") {
                data = [];
            }
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });

    app.get('/mission-control/mgmt-domain/:id', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/mission-control/mgmt-domain', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.put('/mission-control/mgmt-domain/:id', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].update(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/mission-control/mgmt-domain/:id', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].delete(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/mgmt-domain/start-launchpad/:name', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].startLaunchpad(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/mgmt-domain/stop-launchpad/:name', cors(), function(req, res) {
        missionControlAPI["mgmt-domain"].stopLaunchpad(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/cloud-account', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/cloud-account/:id', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/mission-control/cloud-account', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.put('/mission-control/cloud-account/:id', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].update(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/mission-control/cloud-account/:id', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].delete(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //Sdn Get
    app.get('/mission-control/sdn-account', cors(), function(req, res) {
        missionControlAPI["sdn-accounts"].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //Sdn Get individual
    app.get('/mission-control/sdn-account/:id', cors(), function(req, res) {
        missionControlAPI["sdn-accounts"].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //Sdn Create Account
    app.post('/mission-control/sdn-account', cors(), function(req, res) {
        missionControlAPI["sdn-accounts"].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //Sdn update account
    app.put('/mission-control/sdn-account/:id', cors(), function(req, res) {
        missionControlAPI["sdn-accounts"].update(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    //Sdn delete account
    app.delete('/mission-control/sdn-account/:id', cors(), function(req, res) {
        missionControlAPI["sdn-accounts"].delete(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/get-pools-by-cloud-account', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].getPools(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/mission-control/pool', cors(), function(req, res) {
        var poolType = req.body.type;
        var poolCall = "";
        switch (poolType) {
            case "network":
                poolCall = "network-pools";
                break;
            case "vm":
                poolCall = "vm-pools";
                break;
        }
        missionControlAPI[poolCall].create(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/mission-control/pool', cors(), function(req, res) {
        var poolType = req.body.type;
        var poolCall = "";
        switch (poolType) {
            case "network":
                poolCall = "network-pools";
                break;
            case "vm":
                poolCall = "vm-pools";
                break;
        }
        missionControlAPI[poolCall].delete(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    // why is this a POST instead of PUT?
    app.put('/mission-control/pool', cors(), function(req, res) {
        var poolType = req.body.type;
        var poolType = poolType + "-pools";
        missionControlAPI[poolType].edit(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/pool', cors(), function(req, res) {
        missionControlAPI['pools'].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/pool/:type', cors(), function(req, res) {
        var poolType = req.params.type + "-pools";
        missionControlAPI[poolType].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/pool/:type/:poolId', cors(), function(req, res) {
        var poolType = req.params.type + "-pools";
        missionControlAPI[poolType].get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/all-resources', cors(), function(req, res) {
        missionControlAPI['cloud-accounts'].getResources(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/mission-control/crash-details', cors(), function(req, res) {
      debugAPI["crash-details"].get(req).then(
        function(data) {
          res.send(data);
        },
        function (error) {
          sendErrorResponse(error, res);
        }
      );
    });
    app.get('/mission-control/about', cors(), function(req, res) {
      aboutAPI["about"].get(req).then(
        function(data) {
          res.send(data);
        },
        function (error) {
          sendErrorResponse(error, res);
        }
      );
    });
    // End mission control page APIs
    // Start Logging API
    app.get('/logging/syslog-viewer', cors(), function(req, res) {
        logging.get(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });
    // End Logging API

    // Start socket API
    app.post('/socket-polling', cors(), function(req, res) {
        socketManager.subscribe(req).then(function(data) {
          sendSuccessResponse(data, res);
        }, function(error) {
          sendErrorResponse(error, res);
        });
    });
    // End socket API

    // passthrough
    app.get('/api/operational/vnfr-catalog/vnfr/:id', cors(), function(req, res) {
        launchpadAPI['rift'].api(req).then(function(data) {
            res.send(data);
        })
    });
    app.get('/api/operational/ns-instance-opdata/nsr/:id', cors(), function(req, res) {
        launchpadAPI['rift'].api(req).then(function(data) {
            res.send(data);
        })
    });
    app.get('/api/operational/vnfr-catalog/vnfr/:vnfr_id/vdur/:vdur_id', cors(), function(req, res) {
        launchpadAPI['rift'].api(req).then(function(data) {
            res.send(data);
        })
    });
    app.get('/api/operational/restconf-state/streams', cors(), function(req, res) {
        launchpadAPI['rift'].api(req).then(function(data) {
            res.send(data);
        }, function(error) {
            sendErrorResponse(error, res);
        })
    });

    //Start Cloud Account API

    app.get('/cloud-account', cors(), function(req, res) {
        cloudAPI.get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/cloud-account/:id', cors(), function(req, res) {
        cloudAPI.get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/cloud-account', cors(), function(req, res) {
        cloudAPI.create(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.put('/cloud-account/:id', cors(), function(req, res) {
        cloudAPI.update(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/cloud-account/:id', cors(), function(req, res) {
       cloudAPI.delete(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    // end Cloud Account API

    // Start Sdn Account API
    app.get('/sdn-account', cors(), function(req, res) {
        sdnAPI.get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.get('/sdn-account/:id', cors(), function(req, res) {
        sdnAPI.get(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.post('/sdn-account', cors(), function(req, res) {
        sdnAPI.create(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.put('/sdn-account/:id', cors(), function(req, res) {
        sdnAPI.update(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    app.delete('/sdn-account/:id', cors(), function(req, res) {
       sdnAPI.delete(req).then(function(data) {
            sendSuccessResponse(data, res);
        }, function(error) {
            sendErrorResponse(error, res);
        });
    });
    // End Sdn Account API
};
module.exports = routes;
