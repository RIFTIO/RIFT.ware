
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var request = require('../utils/utils.js').request;
var Promise = require('bluebird');
var rp = require('request-promise');
var utils = require('../utils/utils.js');
var constants = require('../common/constants.js')
var _ = require('underscore');
var epa_aggregator = require('./epa_aggregator.js');
var transforms = require('./transforms.js');
var uuid = require('node-uuid');

// Revealing module pattern objects
var Catalog = {};
var Config = {};
var NSR = {};
var VNFR = {};
var RIFT = {};
var ComputeTopology = {};
var NetworkTopology = {};
var VDUR = {};
var CloudAccount = {};
var ConfigAgentAccount = {};
var RPC = {};

// API Configuration Info
var APIConfig = {}
APIConfig.NfviMetrics = ['vcpu', 'memory'];

RPC.executeNSConfigPrimitive = function(req) {
    var api_server = req.query['api_server'];
    return new Promise(function(resolve, reject) {
        var jsonData = {
            "input": req.body
        };

        var headers = _.extend({},
            constants.HTTP_HEADERS.accept.data,
            constants.HTTP_HEADERS.content_type.data, {
                'Authorization': req.get('Authorization')
            }
        );
        request({
            url: utils.confdPort(api_server) + '/api/operations/exec-ns-config-primitive',
            method: 'POST',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData
        }, function(error, response, body) {
            if (utils.validateResponse('RPC.executeNSConfigPrimitive', error, response, body, resolve, reject)) {
                return resolve({
                    statusCode: response.statusCode,
                    data: JSON.stringify(response.body)
                });
            }
        })
    });
};

RPC.getNSConfigPrimitiveValues = function(req) {
    var api_server = req.query['api_server'];
    // var nsr_id = req.body['nsr_id_ref'];
    // var nsConfigPrimitiveName = req.body['name'];
    return new Promise(function(resolve, reject) {
        var jsonData = {
            "input": req.body
        };

        var headers = _.extend({},
            constants.HTTP_HEADERS.accept.data,
            constants.HTTP_HEADERS.content_type.data, {
                'Authorization': req.get('Authorization')
            }
        );
        request({
            uri: utils.confdPort(api_server) + '/api/operations/get-ns-config-primitive-values',
            method: 'POST',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData
        }, function(error, response, body) {
            if (utils.validateResponse('RPC.getNSConfigPrimitiveValues', error, response, body, resolve, reject)) {

                resolve({
                    statusCode: response.statusCode,
                    data: JSON.parse(body)
                });
            }
        });
    }).catch(function(error) {
        console.log('error getting primitive values');
    });
};


var DataCenters = {};
// Catalog module methods
Catalog.get = function(req) {
    var api_server = req.query['api_server'];
    var results = {}
    return new Promise(function(resolve, reject) {
        Promise.all([
            rp({
                uri: utils.confdPort(api_server) + '/api/config/nsd-catalog/nsd?deep',
                method: 'GET',
                headers: _.extend({}, constants.HTTP_HEADERS.accept.collection, {
                    'Authorization': req.get('Authorization')
                }),
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
                resolveWithFullResponse: true
            }),
            rp({
                uri: utils.confdPort(api_server) + '/api/config/vnfd-catalog/vnfd?deep',
                method: 'GET',
                headers: _.extend({}, constants.HTTP_HEADERS.accept.collection, {
                    'Authorization': req.get('Authorization')
                }),
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
                resolveWithFullResponse: true
            }),
            rp({
                uri: utils.confdPort(api_server) + '/api/operational/ns-instance-opdata?deep',
                method: 'GET',
                headers: _.extend({}, constants.HTTP_HEADERS.accept.data, {
                    'Authorization': req.get('Authorization')
                }),
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
                resolveWithFullResponse: true
            })
            // Not enabled for now
            // rp({
            //   uri: utils.confdPort(api_server) + '/api/config/pnfd-catalog/pnfd?deep',
            //   method: 'GET',
            //   headers: _.extend({},
            //     constants.HTTP_HEADERS.accept.collection,
            //     {
            //       'Authorization': req.get('Authorization')
            //     }),
            //   forever: constants.FOREVER_ON,
            // rejectUnauthorized: false,
            //   resolveWithFullResponse: true
            // })
        ]).then(function(result) {
            console.log('Resolved all request promises (NSD, VNFD) successfully');
            var response = [{
                "id": "GUID-1",
                "name": "RIFT.ware™ NS Descriptors Catalog",
                "short-name": "rift.ware-nsd-cat",
                "description": "RIFT.ware™, an open source NFV development and deployment software platform that makes it simple to create, deploy and manage hyper-scale Virtual network functions and applications.",
                "vendor": "RIFT.io",
                "version": "",
                "created-on": "",
                "type": "nsd",
                "meta": {
                    "icon-svg": "data:image/svg+xml,%3C%3Fxml%20version%3D%221.0%22%20encoding%3D%22iso-8859-1%22%3F%3E%0A%3C!--%20Generator%3A%20Adobe%20Illustrator%2018.0.0%2C%20SVG%20Export%20Plug-In%20.%20SVG%20Version%3A%206.00%20Build%200)%20%20--%3E%0A%3C!DOCTYPE%20svg%20PUBLIC%20%22-%2F%2FW3C%2F%2FDTD%20SVG%201.1%2F%2FEN%22%20%22http%3A%2F%2Fwww.w3.org%2FGraphics%2FSVG%2F1.1%2FDTD%2Fsvg11.dtd%22%3E%0A%3Csvg%20version%3D%221.1%22%20id%3D%22connection-icon-1%22%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20xmlns%3Axlink%3D%22http%3A%2F%2Fwww.w3.org%2F1999%2Fxlink%22%20x%3D%220px%22%20y%3D%220px%22%0A%09%20viewBox%3D%220%200%2050%2050%22%20style%3D%22enable-background%3Anew%200%200%2050%2050%3B%22%20xml%3Aspace%3D%22preserve%22%3E%0A%09%3Cpath%20d%3D%22M15%2030c-2.8%200-5-2.2-5-5s2.2-5%205-5%205%202.2%205%205-2.2%205-5%205zm0-8c-1.7%200-3%201.3-3%203s1.3%203%203%203%203-1.3%203-3-1.3-3-3-3z%22%2F%3E%3Cpath%20d%3D%22M35%2020c-2.8%200-5-2.2-5-5s2.2-5%205-5%205%202.2%205%205-2.2%205-5%205zm0-8c-1.7%200-3%201.3-3%203s1.3%203%203%203%203-1.3%203-3-1.3-3-3-3z%22%2F%3E%3Cpath%20d%3D%22M35%2040c-2.8%200-5-2.2-5-5s2.2-5%205-5%205%202.2%205%205-2.2%205-5%205zm0-8c-1.7%200-3%201.3-3%203s1.3%203%203%203%203-1.3%203-3-1.3-3-3-3z%22%2F%3E%3Cpath%20d%3D%22M19.007%2025.885l12.88%206.44-.895%201.788-12.88-6.44z%22%2F%3E%3Cpath%20d%3D%22M30.993%2015.885l.894%201.79-12.88%206.438-.894-1.79z%22%2F%3E%3C%2Fsvg%3E"
                },
                "descriptors": []
            }, {
                "id": "GUID-2",
                "name": "RIFT.ware™ VNF Descriptors Catalog",
                "short-name": "rift.ware-vnfd-cat",
                "description": "RIFT.ware™, an open source NFV development and deployment software platform that makes it simple to create, deploy and manage hyper-scale Virtual network functions and applications.",
                "vendor": "RIFT.io",
                "version": "",
                "created-on": "",
                "type": "vnfd",
                "meta": {
                    "icon-svg": "data:image/svg+xml,<?xml version=\"1.0\" encoding=\"utf-8\"?> <!-- Generator: Adobe Illustrator 16.0.0, SVG Export Plug-In . SVG Version: 6.00 Build 0) --> <!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"> <svg version=\"1.1\" id=\"Layer_3\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\" width=\"100px\" height=\"100px\" viewBox=\"0 0 100 100\" enable-background=\"new 0 0 100 100\" xml:space=\"preserve\"> <g> <path d=\"M58.852,62.447l-4.662-1.033c-0.047-3.138-0.719-6.168-1.996-9.007l3.606-2.92c0.858-0.695,0.99-1.954,0.296-2.813 l-4.521-5.584c-0.334-0.413-0.818-0.675-1.346-0.731c-0.525-0.057-1.056,0.102-1.468,0.435L45.25,43.64v0 c-2.486-1.907-5.277-3.259-8.297-4.019v-4.458c0-1.104-0.896-2-2-2H27.77c-1.104,0-2,0.896-2,2v4.461 c-3.08,0.777-5.922,2.171-8.447,4.144l-3.545-2.82c-0.415-0.33-0.94-0.479-1.472-0.422c-0.527,0.06-1.009,0.327-1.339,0.743 l-4.472,5.623c-0.688,0.864-0.544,2.123,0.32,2.81l3.642,2.896v0c-1.25,2.848-1.895,5.88-1.916,9.011l-4.666,1.078 c-1.076,0.249-1.747,1.322-1.499,2.398l1.616,7.001c0.249,1.077,1.325,1.747,2.399,1.499l4.813-1.111v0 c1.429,2.681,3.344,5.017,5.691,6.943l-2.17,4.55c-0.476,0.997-0.054,2.19,0.943,2.666l6.484,3.094 c0.271,0.129,0.566,0.195,0.861,0.195c0.226,0,0.451-0.038,0.668-0.115c0.5-0.177,0.909-0.545,1.138-1.024l2.198-4.611 c2.923,0.563,5.966,0.554,8.879-0.033l2.236,4.585c0.484,0.994,1.685,1.403,2.675,0.921l6.456-3.148 c0.992-0.484,1.405-1.682,0.921-2.674l-2.206-4.524c2.335-1.946,4.231-4.301,5.639-6.999l4.812,1.067 c1.076,0.237,2.146-0.441,2.385-1.52l1.556-7.014c0.115-0.518,0.02-1.06-0.266-1.508C59.82,62.878,59.369,62.562,58.852,62.447z M40.18,61.761c0,4.859-3.953,8.812-8.813,8.812c-4.858,0-8.811-3.953-8.811-8.812s3.952-8.812,8.811-8.812 C36.227,52.949,40.18,56.902,40.18,61.761z\"/> <path d=\"M64.268,45.324c0.746,0,1.463-0.42,1.806-1.139l1.054-2.208c1.826,0.353,3.736,0.345,5.551-0.021l1.07,2.195 c0.484,0.992,1.682,1.405,2.675,0.921l2.691-1.313c0.477-0.233,0.842-0.646,1.015-1.147c0.172-0.501,0.139-1.051-0.095-1.528 l-1.052-2.155c1.458-1.214,2.645-2.686,3.527-4.377l2.278,0.504c1.075,0.238,2.146-0.442,2.386-1.52l0.647-2.923 c0.238-1.078-0.442-2.146-1.521-2.385l-2.184-0.484c-0.028-1.962-0.449-3.857-1.248-5.632l1.673-1.355 c0.412-0.334,0.675-0.818,0.73-1.345s-0.102-1.056-0.436-1.468l-1.884-2.327c-0.697-0.859-1.957-0.99-2.813-0.295l-1.614,1.307 c-1.554-1.193-3.299-2.038-5.188-2.513v-2.039c0-1.104-0.896-2-2-2h-2.994c-1.104,0-2,0.896-2,2v2.04 c-1.927,0.486-3.703,1.358-5.28,2.592l-1.634-1.298c-0.862-0.687-2.12-0.543-2.81,0.32l-1.864,2.344 c-0.33,0.416-0.481,0.945-0.422,1.472c0.061,0.527,0.327,1.009,0.743,1.339l1.69,1.345c-0.78,1.779-1.184,3.676-1.197,5.636 l-2.189,0.505c-0.517,0.119-0.965,0.439-1.246,0.889c-0.281,0.45-0.372,0.993-0.252,1.51l0.675,2.918 c0.249,1.076,1.323,1.747,2.398,1.498l2.28-0.527c0.892,1.676,2.089,3.137,3.559,4.343l-1.035,2.17 c-0.228,0.479-0.257,1.028-0.08,1.528c0.178,0.5,0.546,0.91,1.024,1.138l2.703,1.289C63.686,45.261,63.979,45.324,64.268,45.324z M64.334,27.961c0-3.039,2.473-5.51,5.512-5.51c3.038,0,5.51,2.472,5.51,5.51c0,3.039-2.472,5.511-5.51,5.511 C66.807,33.472,64.334,31,64.334,27.961z\"/> <path d=\"M96.107,66.441l-2.182-0.484c-0.028-1.961-0.449-3.856-1.25-5.632l1.675-1.355c0.412-0.334,0.675-0.818,0.73-1.346 c0.056-0.527-0.102-1.056-0.436-1.468l-1.885-2.327c-0.695-0.859-1.956-0.99-2.813-0.295l-1.614,1.307 c-1.555-1.193-3.3-2.038-5.188-2.513v-2.039c0-1.104-0.896-2-2-2h-2.994c-1.104,0-2,0.896-2,2v2.041 c-1.929,0.486-3.706,1.358-5.282,2.592l-0.001,0l-1.631-1.298c-0.415-0.331-0.938-0.482-1.472-0.422 c-0.527,0.06-1.009,0.327-1.339,0.742l-1.863,2.343c-0.688,0.865-0.544,2.123,0.32,2.811l1.691,1.345 c-0.782,1.784-1.186,3.68-1.199,5.636l-2.188,0.505c-0.517,0.12-0.965,0.439-1.246,0.889c-0.281,0.45-0.372,0.993-0.252,1.51 l0.675,2.918c0.249,1.076,1.327,1.744,2.397,1.498l2.281-0.526c0.893,1.677,2.09,3.138,3.558,4.343h0.001l-1.035,2.168 c-0.229,0.479-0.258,1.029-0.081,1.529c0.178,0.5,0.546,0.909,1.024,1.138l2.702,1.289c0.278,0.132,0.571,0.195,0.86,0.195 c0.746,0,1.463-0.42,1.806-1.139l1.054-2.208c1.828,0.353,3.739,0.347,5.552-0.021l1.071,2.194 c0.484,0.992,1.682,1.405,2.675,0.921l2.69-1.312c0.477-0.233,0.842-0.645,1.014-1.147c0.173-0.501,0.14-1.051-0.093-1.528 l-1.052-2.155c1.459-1.215,2.645-2.688,3.525-4.377l2.278,0.505c0.52,0.116,1.061,0.02,1.508-0.266 c0.447-0.285,0.763-0.736,0.878-1.254l0.647-2.923C97.866,67.748,97.186,66.681,96.107,66.441z M85.162,66.174 c0,3.039-2.471,5.511-5.508,5.511c-3.039,0-5.512-2.472-5.512-5.511c0-3.039,2.473-5.511,5.512-5.511 C82.691,60.664,85.162,63.136,85.162,66.174z\"/> </g> </svg> "
                },
                "descriptors": []
            }, {
                "id": "GUID-3",
                "name": "RIFT.ware™ PNF Descriptors Catalog",
                "short-name": "rift.ware-pnfd-cat",
                "description": "RIFT.ware™, an open source NFV development and deployment software platform that makes it simple to create, deploy and manage hyper-scale Virtual network functions and applications.",
                "vendor": "RIFT.io",
                "version": "",
                "created-on": "",
                "type": "pnfd",
                "meta": {
                    "icon-svg": "data:image/svg+xml,<?xml version=\"1.0\" encoding=\"utf-8\"?> <!-- Generator: Adobe Illustrator 16.0.0, SVG Export Plug-In . SVG Version: 6.00 Build 0) --> <!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"> <svg version=\"1.1\" id=\"Layer_4\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\" width=\"100px\" height=\"100px\" viewBox=\"0 0 100 100\" enable-background=\"new 0 0 100 100\" xml:space=\"preserve\"> <path d=\"M86.334,47.444V35.759H13.666v11.686h3.561v5.111h-3.561v11.686h72.668V52.556h-4.108v-5.111H86.334z M26.628,59.454h-5.051 v-4.941h5.051V59.454z M26.628,52.404h-5.051v-4.941h5.051V52.404z M26.628,45.486h-5.051v-4.941h5.051V45.486z M34.094,59.454 h-5.051v-4.941h5.051V59.454z M34.094,52.404h-5.051v-4.941h5.051V52.404z M34.094,45.486h-5.051v-4.941h5.051V45.486z M41.452,59.454h-5.051v-4.941h5.051V59.454z M41.452,52.404h-5.051v-4.941h5.051V52.404z M41.452,45.486h-5.051v-4.941h5.051 V45.486z M48.733,59.454h-5.051v-4.941h5.051V59.454z M48.733,52.404h-5.051v-4.941h5.051V52.404z M48.733,45.486h-5.051v-4.941 h5.051V45.486z M56.2,59.454h-5.051v-4.941H56.2V59.454z M56.2,52.404h-5.051v-4.941H56.2V52.404z M56.2,45.486h-5.051v-4.941H56.2 V45.486z M63.558,59.454h-5.05v-4.941h5.05V59.454z M63.558,52.404h-5.05v-4.941h5.05V52.404z M63.558,45.486h-5.05v-4.941h5.05 V45.486z M74.858,59.312h-6.521v-3.013h6.521V59.312z M71.572,50.854c-2.875,0-5.204-2.33-5.204-5.203s2.329-5.203,5.204-5.203 s5.204,2.33,5.204,5.203S74.446,50.854,71.572,50.854z M74.858,45.618c0,1.801-1.46,3.261-3.261,3.261 c-1.8,0-3.261-1.46-3.261-3.261s1.46-3.26,3.261-3.26C73.398,42.358,74.858,43.817,74.858,45.618z\"/> </svg>"
                },
                "descriptors": []
            }];
            if (result[0].body) {
                response[0].descriptors = JSON.parse(result[0].body).collection['nsd:nsd'];
                if (result[2].body) {
                    var data = JSON.parse(result[2].body);
                    if (data && data["nsr:ns-instance-opdata"] && data["nsr:ns-instance-opdata"]["rw-nsr:nsd-ref-count"]) {
                        var nsdRefCountCollection = data["nsr:ns-instance-opdata"]["rw-nsr:nsd-ref-count"];
                        response[0].descriptors.map(function(nsd) {
                            if (!nsd["meta"]) {
                                nsd["meta"] = {};
                            }
                            if (typeof nsd['meta'] == 'string') {
                                nsd['meta'] = JSON.parse(nsd['meta']);
                            }
                            nsd["meta"]["instance-ref-count"] = _.findWhere(nsdRefCountCollection, {
                                "nsd-id-ref": nsd.id
                            })["instance-ref-count"];
                        });
                    }
                }
            };
            if (result[1].body) {
                response[1].descriptors = JSON.parse(result[1].body).collection['vnfd:vnfd'];
            };
            // if (result[2].body) {
            //   response[2].descriptors = JSON.parse(result[2].body).collection['pnfd:pnfd'];
            // };
            resolve({
                statusCode: response.statusCode || 200,
                data: JSON.stringify(response)
            });
        }).catch(function(error) {
            // Todo: Need better logic than all or nothing.
            // Right now even if one of the southbound APIs fails - all fail
            var res = {};
            console.log('Problem with Catalog.get', error);
            res.statusCode = error.statusCode || 500;
            res.errorMessage = {
                error: 'Failed to get catalogs' + error
            };
            reject(res);
        });
    });
};
Catalog.delete = function(req) {
    var api_server = req.query['api_server'];
    var catalogType = req.params.catalogType;
    var id = req.params.id;
    console.log('Deleting', catalogType, id, 'from', api_server);
    return new Promise(function(resolve, reject) {
        request({
            uri: utils.confdPort(api_server) + '/api/config/' + catalogType + '-catalog/' + catalogType + '/' + id,
            method: 'DELETE',
            headers: _.extend({}, constants.HTTP_HEADERS.accept.data, {
                'Authorization': req.get('Authorization')
            }),
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('Catalog.delete', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode
                });
            }
        });
    });
};
Catalog.getVNFD = function(req) {
    var api_server = req.query['api_server'];
    var vnfdID = req.body.data;
    var authorization = req.get('Authorization');
    var VNFDs = [];
    if (typeof(vnfdID) == "object" && vnfdID.constructor.name == "Array") {
        vnfdID.map(function(id) {
            VNFDs.push(requestVNFD(id));
        });
    } else {
        VNFDs.push(requestVNFD(vnfdID));
    }
    return new Promise(function(resolve, reject) {
        Promise.all(VNFDs).then(function(data) {
            resolve(data)
        }).catch(function(error) {
            // Todo: Need better logic than all or nothing.
            // Right now even if one of the southbound APIs fails - all fail
            var res = {};
            console.log('Problem with Catalog.getVNFD', error);
            res.statusCode = 404;
            res.errorMessage = {
                error: 'Failed to get VNFDs' + error
            };
            reject(res);
        });
    });

    function requestVNFD(id) {
        return new Promise(function(resolve, reject) {
            var url = utils.confdPort(api_server) + '/api/config/vnfd-catalog/vnfd' + (id ? '/' + id : '') + '?deep';
            request({
                uri: url,
                method: 'GET',
                headers: _.extend({}, constants.HTTP_HEADERS.accept.data, {
                    'Authorization': authorization
                }),
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
            }, function(error, response, body) {
                if (utils.validateResponse('Catalog.create', error, response, body, resolve, reject)) {
                    var data;
                    //Is this still needed?
                    try {
                        data = JSON.parse(response.body)
                    } catch (e) {
                        reject({
                            statusCode: response ? response.statusCode : 400,
                            errorMessage: 'Issue parsing VNFD ' + id + 'from ' + utils.confdPort(api_server) + '/api/config/vnfd-catalog/vnfd/' + id + '?deep'
                        });
                    }
                    resolve(data);
                }
            });
        });
    }
};
Catalog.create = function(req) {
    var api_server = req.query['api_server'];
    var catalogType = req.params.catalogType;
    var data = req.body;
    console.log('Creating', catalogType, 'on', api_server);
    var jsonData = {};
    jsonData[catalogType] = [];
    jsonData[catalogType].push(data);
    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders, constants.HTTP_HEADERS.accept.data, constants.HTTP_HEADERS.content_type.data, {
            'Authorization': req.get('Authorization')
        });
        request({
            uri: utils.confdPort(api_server) + '/api/config/' + catalogType + '-catalog',
            method: 'POST',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData
        }, function(error, response, body) {
            if (utils.validateResponse('Catalog.create', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode
                });
            }
        });
    });
};
Catalog.update = function(req) {
    var api_server = req.query['api_server'];
    var catalogType = req.params.catalogType;
    var id = req.params.id;
    var data = req.body;
    console.log('Updating', catalogType, 'id', id, 'on', api_server);
    var jsonData = {};
    jsonData[catalogType] = {};
    jsonData[catalogType] = data;
    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders, constants.HTTP_HEADERS.accept.data, constants.HTTP_HEADERS.content_type.data, {
            'Authorization': req.get('Authorization')
        });
        request({
            uri: utils.confdPort(api_server) + '/api/config/' + catalogType + '-catalog' + '/' + catalogType + '/' + id,
            method: 'PUT',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData
        }, function(error, response, body) {
            if (utils.validateResponse('Catalog.update', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode
                });
            }
        });
    });
};

// NSR module methods
// Spend some time refactoring this
// refactor to accept only request object
NSR.get = function(req) {
    var self = this;
    var nsrPromises = [];
    var api_server = req.query["api_server"];
    var id = req.params.id;
    var nsdInfo = new Promise(function(resolve, reject) {
        request({
            uri: utils.confdPort(api_server) + '/api/config/nsd-catalog/nsd?deep',
            method: 'GET',
            headers: _.extend({}, constants.HTTP_HEADERS.accept.collection, {
                'Authorization': req.get('Authorization')
            }),
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.get nsd-catalog', error, response, body, resolve, reject)) {
                var data;
                var isString = typeof(response.body) == "string";
                if (isString && response.body == '') return resolve('empty');
                data = isString ? JSON.parse(response.body) : response.body;
                var nsdData = data.collection["nsd:nsd"];
                if (nsdData.constructor.name == "Object") {
                    nsdData = [nsdData];
                }
                resolve(nsdData);
            };
        })
    })
    var config = new Promise(function(resolve, reject) {
        request({
            uri: utils.confdPort(api_server) + '/api/operational/ns-instance-config/nsr' + (id ? '/' + id : '') + '?deep',
            method: 'GET',
            headers: _.extend({}, id ? constants.HTTP_HEADERS.accept.data : constants.HTTP_HEADERS.accept.collection, {
                'Authorization': req.get('Authorization')
            }),
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.get ns-instance-config', error, response, body, resolve, reject)) {
                var data;
                var isString = typeof(response.body) == "string";
                if (isString && response.body == '') return resolve();
                data = isString ? JSON.parse(response.body) : response.body;
                data = id ? data : data.collection;
                var nsrData = data["nsr:nsr"];
                if (nsrData.constructor.name == "Object") {
                    nsrData = [nsrData];
                }
                resolve(nsrData);
            };
        });
    });
    var opData = new Promise(function(resolve, reject) {
        request({
            uri: utils.confdPort(api_server) + '/api/operational/ns-instance-opdata/nsr' + (id ? '/' + id : '') + '?deep',
            method: 'GET',
            headers: _.extend({}, id ? constants.HTTP_HEADERS.accept.data : constants.HTTP_HEADERS.accept.collection, {
                'Authorization': req.get('Authorization')
            }),
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.get ns-instance-opdata', error, response, body, resolve, reject)) {
                var data;
                var isString = typeof(response.body) == "string";
                if (isString && response.body == '') return resolve();
                data = isString ? JSON.parse(response.body) : response.body;
                data = id ? data : data.collection;
                var nsrData = data["nsr:nsr"];
                if (nsrData.constructor.name == "Object") {
                    nsrData = [nsrData];
                }
                nsrData.forEach(self.decorateWithScalingGroupDict);
                nsrData.forEach(self.decorateAndTransformNFVI);
                nsrData.forEach(self.decorateAndTransformWithControls);
                nsrData.forEach(self.decorateAndTransformWithMonitoringParams);
                // Hacky fix to add VNFR short names to monitoring params until it can be added to Yang
                // Also adds EPA params
                Promise.all(self.addVnfrDataPromise(req, nsrData)).then(function() {
                    resolve(nsrData);
                })
            };
        });
    }).catch(function(error) {
        console.log('error getting aggregated NS opdata', error)
        //note this will actually trigger the success callback
    });
    return new Promise(function(resolve, reject) {
        //Need smarter error handling here
        Promise.all([nsdInfo, config, opData]).then(function(resolves) {
            var aggregate = {};
            var nsdNames = {};
            // resolves[0] ==> nsd
            // resolves[1] ==> ns-instance-config
            // resolves[2] ==> ns-instance-opdata
            if (!resolves[0] || !resolves[1] && !resolves[2]) return resolve({
                nsrs: []
            });
            resolves[0].forEach(function(v, k) {
                nsdNames[v.id] = v.name;
            })
            resolves[1].forEach(function(v, k) {
                v.nsd_name = nsdNames[v["nsd-ref"]];
                var scaling_group_descriptor = null;

                scaling_group_descriptor = _.findWhere(resolves[0], {
                    id:v['nsd-ref']
                })['scaling-group-descriptor'];

                if (scaling_group_descriptor) {
                    scaling_group_descriptor.map(function(sgd, sgdi) {
                        sgd['vnfd-member'] && sgd['vnfd-member'].map(function(vnfd, vnfdi) {
                            var vnfrObj = _.findWhere(_.findWhere(resolves[2], {
                                    'ns-instance-config-ref': v.id
                                }).vnfrs, {
                                'member-vnf-index-ref': vnfd['member-vnf-index-ref']
                            });
                            if (vnfrObj) {
                                vnfd['short-name'] = vnfrObj['short-name'];
                            }
                        })
                    })
                    v['scaling-group-descriptor'] = scaling_group_descriptor;
                }

                if (resolves[2] && resolves[2].constructor.name == "Array") {
                    resolves[2].forEach(function(w, l) {
                        if (v.id == w["ns-instance-config-ref"]) {
                            for (prop in w) {
                                if (prop != "ns-instance-config-ref" && !v.hasOwnProperty(prop)) {
                                    v[prop] = w[prop];
                                }
                            }
                        }
                    });
                }

                v['scaling-group-record'] && v['scaling-group-record'].map(function(sgr) {
                    var scalingGroupName = sgr['scaling-group-name-ref'];
                    sgr['instance'] && sgr['instance'].map(function(instance) {
                        var scalingGroupInstanceId = instance['instance-id'];
                        instance['vnfrs'] && instance['vnfrs'].map(function(vnfr) {
                            var vnfrObj = _.findWhere(v['vnfrs'], {id: vnfr});
                            if (vnfrObj) {
                                vnfrObj['scaling-group-name'] = scalingGroupName;
                                vnfrObj['scaling-group-instance-id'] = scalingGroupInstanceId;
                            }
                        });
                    });
                })
            });
            var nsrsData = resolves[1];
            nsrsData.sort(function(a, b) {
                return a["create-time"] - b["create-time"];
            });
            resolve({
                nsrs: nsrsData
            });
        }).catch(function(error) {
            reject({
                statusCode: 404,
                errorMessage: error
            })
        })
    });
};
// Static VNFR Cache bu VNFR ID
var staticVNFRCache = {};

/**
 * [decorateWithScalingGroupDict description]
 * @param  {[type]} nsr [description]
 * @return {[type]}
{vnfr-id} : {
    "scaling-group-name-ref": "sg1",
    "instance-id": 0,
    "op-status": "running",
    "is-default": "true",
    "create-time": 1463593760,
    "config-status": "configuring",
    "vnfrs": [
        "432154e3-164e-4c05-83ee-3b56e4c898e7"
    ]
}
 */
NSR.decorateWithScalingGroupDict = function(nsr) {
    var sg = nsr["scaling-group-record"];
    var dict = {};
    if(sg) {
        sg.map(function(s) {
            var sgRef = s['scaling-group-name-ref'];
            s.instance && s.instance.map(function(si) {
                si.vnfrs && si.vnfrs.map(function(v) {
                    dict[v] = si;
                    dict[v]["scaling-group-name-ref"] = sgRef;
                })
            })
        })
    }
    return nsr['vnfr-scaling-groups'] = dict;
}
NSR.addVnfrDataPromise = function(req, nsrs) {
    var api_server = req.query['api_server'];
    var promises = [];
    nsrs.map(function(nsr) {
        var epa_params = {};
        var constituent_vnfr_ref = nsr["constituent-vnfr-ref"];
        var vnfrPromises = [];
        nsr["vnfrs"] = [];
        nsr["dashboard-urls"] = [];
        nsr['nfvi-metrics'] = [];
        if (!constituent_vnfr_ref) {
            console.log('Something is wrong, there are no constituent-vnfr-refs');
            constituent_vnfr_ref = [];
        }
        //Get VNFR Static Data
        constituent_vnfr_ref && constituent_vnfr_ref.map(function(constituentVnfrObj) {
            req.params.id = constituentVnfrObj['vnfr-id'];
            var vnfrPromise;
            vnfrPromise = VNFR.get(req).then(function(vnfr) {
                try {
                    var vnfrItem = vnfr[0];
                    decorateNSRWithVNFR(nsr, vnfrItem)
                    staticVNFRCache[vnfrItem.id] = vnfrItem;
                } catch (e) {
                    console.log('431: Error retreiving VNFR by ID');
                }
            });
            vnfrPromises.push(vnfrPromise);
        });
        var NSR_Promise = new Promise(function(resolve, reject) {
            Promise.all(vnfrPromises).then(function() {
                var vnfrs = staticVNFRCache;
                //Aggregate EPA Params
                constituent_vnfr_ref && constituent_vnfr_ref.map(function(k) {
                        if (vnfrs[k['vnfr-id']]) {
                            epa_params = epa_aggregator(vnfrs[k['vnfr-id']].vdur, epa_params);
                        }
                    })
                //Add VNFR Name to monitoring params
                try {
                    if (nsr["monitoring-param"]) {
                        nsr["monitoring-param"].map(function(m) {
                            var vnfr = vnfrs[m["vnfr-id"]];
                            m["vnfr-name"] = vnfr['name'] ? vnfr['name'] : (vnfr['short-name'] ? vnfr['short-name'] : 'VNFR');
                        });
                    }
                } catch (e) {
                    console.log('457: Something went wrong adding vnfr names to the monitoring params');
                }
                resolve();
            })
        })
        nsr["epa-params"] = epa_params;
        promises.push(NSR_Promise);
    })
    return promises;

    function decorateNSRWithVNFR(nsr, vnfr) {
        var vnfrObj = {
            id: vnfr.id,
            "member-vnf-index-ref": vnfr["member-vnf-index-ref"],
            "short-name": vnfr["short-name"],
            "vnf-configuration": vnfr["vnf-configuration"],
            "nsr-id": nsr['ns-instance-config-ref'],
            "name": vnfr['name'],
            "vdur": vnfr["vdur"]
        };
        var vnfrSg = nsr['vnfr-scaling-groups'];
        var vnfrName = vnfr["name"];
        if(vnfrSg) {
            if(vnfrSg[vnfr.id]) {
                vnfrName = vnfrSg[vnfr.id]["scaling-group-name-ref"] + ':' + vnfrSg[vnfr.id][ "instance-id"] + ':' + vnfrName;
            }
        }
        var vnfrNfviMetrics = buildNfviGraphs(vnfr.vdur, vnfrName);
        if (vnfr['vnf-configuration'] && vnfr['vnf-configuration']['config-primitive'] && vnfr['vnf-configuration']['config-primitive'].length > 0) {
            vnfrObj['config-primitives-present'] = true;
        } else {
            vnfrObj['config-primitives-present'] = false;
        }
		transforms.mergeVnfrNfviMetrics(vnfrNfviMetrics, nsr["nfvi-metrics"]);
        //TODO: Should be sorted by create-time when it becomes available instead of id
        // nsr["vnfrs"].splice(_.sortedIndex(nsr['vnfrs'], vnfrObj, 'create-time'), 0, vnfrObj);
        nsr["vnfrs"].splice(_.sortedIndex(nsr['vnfrs'], vnfrObj, 'id'), 0, vnfrObj);
        vnfrObj["dashboard-url"] = vnfr["dashboard-url"];
        nsr["dashboard-urls"].push(vnfrObj);
    }
}
NSR.create = function(req) {
    var api_server = req.query['api_server'];
    var data = req.body.data;
    console.log('Instantiating NSR on ', api_server);
    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders, constants.HTTP_HEADERS.accept.data, constants.HTTP_HEADERS.content_type.data, {
            'Authorization': req.get('Authorization')
        });
        request({
            uri: utils.confdPort(api_server) + '/api/config/ns-instance-config',
            method: 'POST',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: data
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.create', error, response, body, resolve, reject)) {
                var nsr_id = null;
                try {
                    nsr_id = data.nsr[0].id;
                } catch (e) {
                    console.log("NSR.create unable to get nsr_id. Error: %s",
                    e.toString());
                }
                resolve({
                    statusCode: response.statusCode,
                    data: { nsr_id: nsr_id }
                 });
            };
        });
    });
};
NSR.delete = function(req) {
    var api_server = req.query["api_server"];
    var id = req.params.id;
    if (!id || !api_server) {
        return new Promise(function(resolve, reject) {
            console.log('Must specifiy api_server and id to delete NSR');
            return reject({
                statusCode: 500,
                errorMessage: {
                    error: 'Must specifiy api_server and id to delete NSR'
                }
            });
        });
    };
    console.log('Deleting NSR with id: ' + id + 'on server: ' + api_server);
    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders, constants.HTTP_HEADERS.accept.data, {
            'Authorization': req.get('Authorization')
        });
        request({
            uri: utils.confdPort(api_server) + '/api/config/ns-instance-config/nsr/' + id,
            method: 'DELETE',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.delete', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode,
                    data: JSON.stringify(response.body)
                });
            };
        });
    });
};
NSR.decorateAndTransformWithMonitoringParams = function(nsr) {
    var monitoring_params = [];
    nsr["monitoring-param"] = [];
    try {
        var vnfrmp = nsr['vnf-monitoring-param'];
        vnfrmp.map(function(mpg, i) {
            if (mpg['monitoring-param'] && mpg['monitoring-param'].length > 0) {
                mpg['monitoring-param'].map(function(mp, j) {
                    mp["vnfr-id"] = mpg["vnfr-id-ref"];
                    mp["group-tag"] = (mp["group-tag"] ? mp["group-tag"] : 'Group') + '-' + i;
                    mp["mp-id"] = mp["id"] + '-' + mp["vnfr-id"];
                    monitoring_params.push(mp);
                });
            }
        });
        nsr["monitoring-param"] = monitoring_params;
        delete nsr['vnf-monitoring-param'];
    } catch (e) {
        console.log(e)
    }
    return nsr;
}
NSR.decorateAndTransformNFVI = function(nsr) {
        var toDecorate = [];
        // var metricsToUse = ["vcpu", "memory", "storage", "network"];
        var metricsToUse = ["vcpu", "memory"];
        try {
            var nfviMetrics = nsr["rw-nsr:nfvi-metrics"];
            if (nfviMetrics) {
                metricsToUse.map(function(name) {
                    toDecorate.push(nfviMetrics[name])
                });
            }
            nsr["nfvi-metrics"] = toDecorate;
            delete nsr["rw-nsr:nfvi-metrics"];
        } catch (e) {}
        return nsr;
    }
    //Not a great pattern, Need a better way of handling logging;
    //Refactor and move to the logging/logging.js
var logCache = {
    decorateAndTransformWithControls: {}
}
NSR.decorateAndTransformWithControls = function(nsr) {
    var controlTypes = ["action-param", "control-param"];
    var nsControls = [];
    var Groups = {};
    controlTypes.map(function(control) {
        try {
            var controls = nsr["rw-nsr:" + control];
            // nsControls.push(controls);
            controls.map(function(item) {
                if (!Groups[item["group-tag"]]) {
                    Groups[item["group-tag"]] = {};
                    Groups[item["group-tag"]]["action-param"] = []
                    Groups[item["group-tag"]]["control-param"] = []
                }
                Groups[item["group-tag"]][control].push(item);
            });
            delete nsr["rw-nsr:" + control];
        } catch (e) {
            var id = nsr["ns-instance-config-ref"];
            if (!logCache.decorateAndTransformWithControls[id]) {
                logCache.decorateAndTransformWithControls[id] = {};
            }
            var log = logCache.decorateAndTransformWithControls[id];
            if (!log[control]) {
                log[control] = true;
                console.log('No controls exist for ' + control + ' at ' + nsr["ns-instance-config-ref"]);
            }
        }
    });
    for (k in Groups) {
        var obj = {}
        obj[k] = Groups[k];
        nsControls.push(obj)
    }
    nsr.nsControls = nsControls;
    return nsr;
};
NSR.setStatus = function(req) {
    var api_server = req.query['api_server'];
    var id = req.params.id;
    var status = req.body.status;
    console.log('Setting NSR (id: ' + id + ') status, on ' + api_server + ', to be: ' + status);
    return new Promise(function(resolve, reject) {
        var command;
        if (typeof(status) != "string") {
            reject({
                'ERROR': 'NSR.setStatus Error: status is not a string type'
            });
        }
        command = status.toUpperCase();
        if (command != "ENABLED" && command != "DISABLED") {
            reject({
                'ERROR': 'NSR.setStatus Error: status is: ' + command + '. It should be ENABLED or DISABLED'
            });
        }
        var requestHeaders = {};
        _.extend(requestHeaders, constants.HTTP_HEADERS.accept.data, constants.HTTP_HEADERS.content_type.data, {
            'Authorization': req.get('Authorization')
        });
        request({
            uri: utils.confdPort(api_server) + '/api/config/ns-instance-config/nsr/' + id + '/admin-status/',
            method: 'PUT',
            headers: requestHeaders,
            json: {
                "nsr:admin-status": command
            },
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('NSR.setStatus', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode
                });
            };
        });
    });
};

NSR.createScalingGroupInstance = function(req) {
    var api_server = req.query['api_server'];
    var id = req.params.id;
    var scaling_group_id = req.params.scaling_group_id;
    if (!api_server || !id || !scaling_group_id) {
        return new Promise(function(resolve, reject) {
            return reject({
                statusCode: 500,
                errorMessage: {
                    error: 'API server/NSR id/Scaling group not provided'
                }
            });
        });
    }

    var instance_id = Math.floor(Math.random() * 65535);

    var jsonData = {
        instance: [{
            // id: uuid.v1()
            id: instance_id
        }]
    };

    console.log('Creating scaling group instance for NSR ', id, ', scaling group ', scaling_group_id, ' with instance id ', instance_id);

    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders,
            constants.HTTP_HEADERS.accept.data,
            constants.HTTP_HEADERS.content_type.data,
            {
                'Authorization': req.get('Authorization')
            }
        );

        request({
            uri: utils.confdPort(api_server) + '/api/config/ns-instance-config/nsr/' + id + '/scaling-group/' + scaling_group_id + '/instance',
            method: 'POST',
            headers: requestHeaders,
            json: jsonData,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false
        }, function (error, response, body) {
            if (utils.validateResponse('NSR.createScalingGroupInstance', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode,
                    data: typeof response.body == 'string' ? JSON.parse(response.body):response.body
                })
            }
        });
    });
};

NSR.deleteScalingGroupInstance = function(req) {
    var api_server=req.query['api_server'];
    var id = req.params.id;
    var scaling_group_id = req.params.scaling_group_id;
    var scaling_instance_id = req.params.scaling_instance_id;

    if (!api_server || !id || !scaling_group_id || !scaling_instance_id) {
        return new Promise(function(resolve, reject) {
            return reject({
                statusCode: 500,
                errorMessage: {
                    error: 'API server/NSR id/Scaling group/Scaling instance id not provided'
                }
            });
        });
    }

    console.log('Deleting scaling group instance id ', scaling_instance_id,
        ' for scaling group ', scaling_group_id,
        ', under NSR ', id);

    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders,
            constants.HTTP_HEADERS.accept.data,
            constants.HTTP_HEADERS.content_type.data,
            {
                'Authorization': req.get('Authorization')
            }
        );

        request({
            uri: utils.confdPort(api_server) + '/api/config/ns-instance-config/nsr/' + id + '/scaling-group/' + scaling_group_id + '/instance/' + scaling_instance_id,
            method: 'DELETE',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false
        }, function (error, response, body) {
            if (utils.validateResponse('NSR.deleteScalingGroupInstance', error, response, body, resolve, reject)) {
                resolve({
                    statusCode: response.statusCode,
                    data: typeof response.body == 'string' ? JSON.parse(response.body):response.body
                })
            }
        });
    });
}

VNFR.get = function(req) {
    var api_server = req.query["api_server"];
    var id = req.params.id;
    var uri = utils.confdPort(api_server);
    uri += '/api/operational/vnfr-catalog/vnfr' + (id ? '/' + id : '') + '?deep';
    var headers = _.extend({}, id ? constants.HTTP_HEADERS.accept.data : constants.HTTP_HEADERS.accept.collection, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        request({
            url: uri,
            method: 'GET',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('VNFR.get', error, response, body, resolve, reject)) {
                var data = JSON.parse(response.body);
                var returnData = id ? [data["vnfr:vnfr"]] : data.collection["vnfr:vnfr"];
                returnData.forEach(function(vnfr) {
                    vnfr['nfvi-metrics'] = buildNfviGraphs(vnfr.vdur);
                    vnfr['epa-params'] = epa_aggregator(vnfr.vdur);
                    vnfr['config-primitives-present'] = (vnfr['vnf-configuration'] && vnfr['vnf-configuration']['config-primitive'] && vnfr['vnf-configuration']['config-primitive'].length > 0) ? true : false;
                })
                return resolve(returnData);
            };
        });
    });
}

function buildNfviGraphs(VDURs, vnfrName){
        var temp = {};
        var toReturn = [];
        APIConfig.NfviMetrics.map(function(k) {

            VDURs && VDURs.map(function(v,i) {
                //Check for RIFT-12699: VDUR NFVI Metrics not fully populated
                if (v["rw-vnfr:nfvi-metrics"] && v["rw-vnfr:nfvi-metrics"][k] && v["rw-vnfr:nfvi-metrics"][k].hasOwnProperty('utilization')) {
                    if(!temp[k]) {
                        temp[k] = {
                            title: '',
                            data: []
                        };
                    };
                    try {
                        var data = v["rw-vnfr:nfvi-metrics"][k];
                        var newData = {};
                        newData.name = v.name ? v.name : v.id.substring(0,6);
                        newData.name = vnfrName ? vnfrName + ': ' + newData.name : newData.name;
                        newData.id = v.id;
                        //converts to perentage
                        newData.utilization = data.utilization * 0.01;
                        temp[k].data.push(newData);
                        temp[k].title = v["rw-vnfr:nfvi-metrics"][k].label;
                    } catch (e) {
                        console.log('Something went wrong with the VNFR NFVI Metrics. Check that the data is being properly returned. ERROR: ', e);
                    }
                }
            });
            if(temp[k]) {
                toReturn.push(temp[k]);
            }
        });
        return toReturn;
    }


//Cache NSR reference for VNFR
VNFR.cachedNSR = {};
VNFR.getByNSR = function(req) {
    var api_server = req.query["api_server"];
    var id = req.params.nsr_id;
    var uri = utils.confdPort(api_server);
    var reqClone = _.clone(req);
    delete reqClone.params.id;
    uri += '/api/operational/ns-instance-opdata/nsr/' + id + '?deep';
    var headers = _.extend({}, id ? constants.HTTP_HEADERS.accept.data : constants.HTTP_HEADERS.accept.collection, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        if (VNFR.cachedNSR[id]) {
            var data = VNFR.cachedNSR[id];
            var vnfrList = _.pluck(data["constituent-vnfr-ref"], 'vnfr-id');
            VNFR.get(reqClone).then(function(vnfrData) {
                resolve(filterVnfrByList(vnfrList, vnfrData));
            });
        } else {
            request({
                url: uri,
                method: 'GET',
                headers: headers,
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
            }, function(error, response, body) {
                if (utils.validateResponse('VNFR.getByNSR', error, response, body, resolve, reject)) {
                    var data = JSON.parse(response.body);
                    data = data["nsr:nsr"];
                    //Cache NSR data with NSR-ID as
                    VNFR.cachedNSR[id] = data;
                    var vnfrList = _.pluck(data["constituent-vnfr-ref"], 'vnfr-id');
                    var returnData = [];
                    VNFR.get(reqClone).then(function(vnfrData) {
                        resolve(filterVnfrByList(vnfrList, vnfrData));
                    });
                };
            });
        }
    });
};

function filterVnfrByList(vnfrList, vnfrData) {
    return vnfrData.map(function(vnfr) {
        if (vnfrList.indexOf(vnfr.id) > -1) {
            return vnfr;
        }
    })
};

RIFT.api = function(req) {
    var api_server = req.query["api_server"];
    var uri = utils.confdPort(api_server);
    var url = req.path;
    return new Promise(function(resolve, reject) {
        request({
            url: uri + url + '?deep',
            method: 'GET',
            headers: _.extend({}, constants.HTTP_HEADERS.accept.data, {
                'Authorization': req.get('Authorization')
            }),
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('RIFT.api', error, response, body, resolve, reject)) {
                resolve(JSON.parse(response.body))
            };
        })
    })
};

ComputeTopology.get = function(req) {
    var api_server = req.query['api_server'];
    var nsr_id = req.params.id;
    var result = {
        id: nsr_id, // node id
        name: nsr_id, // node name to display
        parameters: {}, // the parameters that can be used to determine size/color, etc. for the node
        type: 'nsr',
        children: [] // children for the node
    };
    return new Promise(function(resolve, reject) {
        var nsrPromise = new Promise(function(success, failure) {
            request({
                uri: utils.confdPort(api_server) + '/api/operational/ns-instance-opdata/nsr/' + nsr_id + '?deep',
                method: 'GET',
                headers: _.extend({},
                    constants.HTTP_HEADERS.accept.data, {
                        'Authorization': req.get('Authorization')
                    }),
                forever: constants.FOREVER_ON,
                rejectUnauthorized: false,
            }, function(error, response, body) {
                if (utils.validateResponse('ComputeTopology.get ns-instance-opdata/nsr/:id', error, response, body, success, failure)) {
                    var data;
                    var isString = typeof(response.body) == "string";
                    if (isString && response.body == '') {
                        return success({});
                    }
                    try {
                        data = isString ? JSON.parse(response.body) : response.body;

                        var nsrNFVIMetricData = data["nsr:nsr"]["rw-nsr:nfvi-metrics"];
                        result.parameters = nsrNFVIMetricData;

                        result.name = data["nsr:nsr"]["name-ref"];

                        var nsrData = data["nsr:nsr"]["constituent-vnfr-ref"];
                        success(nsrData);
                    } catch (e) {
                        console.log('Error parsing ns-instance-opdata for NSR ID', nsr_id, 'Exception:', e);
                        return failure()
                    }
                };
            });
        }).then(function(data) {

            try {
                // got NSR data
                // now get VNFR data and populate the structure
                var vnfrPromises = [];

                // Run separately to confirm that primary structure is populated before promise resolution takes over
                // and starts modifying the data
                data.forEach(function(vnfrObj) {

		    var vnfrId = vnfrObj['vnfr-id'];

                    // If anything needs to be added to result for each vnfrId, do it here

                    vnfrPromises.push(
                        new Promise(function(success, failure) {
                            rp({
                                uri: utils.confdPort(api_server) + '/api/operational/vnfr-catalog/vnfr/' + vnfrId + '?deep',
                                method: 'GET',
                                headers: _.extend({}, constants.HTTP_HEADERS.accept.data, {
                                    'Authorization': req.get('Authorization')
                                }),
                                forever: constants.FOREVER_ON,
                                rejectUnauthorized: false,
                                resolveWithFullResponse: true
                            }, function(error, response, body) {
                                if (utils.validateResponse('ComputeTopology.get vnfr-catalaog/vnfr/:id', error, response, body, success, failure)) {
                                    try {
                                        var data = JSON.parse(response.body);
                                        var returnData = data["vnfr:vnfr"];

                                        // Push VNFRs in result
                                        result.children.push({
                                            id: vnfrId,
                                            name: returnData.name,
                                            parameters: {}, // nfvi metrics here
                                            children: [],
                                            type: 'vnfr'
                                        });

                                        // Push VDURs in result
                                        returnData.vdur.forEach(function(vdur) {
                                            result.children[result.children.length - 1].children.push({
                                                id: vdur.id,
                                                name: vdur.id,
                                                parameters: {},
                                                type: 'vdur'
                                                    // children: []
                                            });
                                        });

                                        return success(returnData.vdur);
                                    } catch (e) {
                                        console.log('Error parsing vnfr-catalog for VNFR ID', vnfrId, 'Exception:', e);
                                        return failure();
                                    }
                                };
                            });
                        })
                    );
                });

                Promise.all(vnfrPromises).then(function(output) {
                    console.log('Resolved all VNFR requests successfully');
                    // By now result must be completely populated. output is moot

                    // Sort the results as there's no order to them from RIFT-REST
                    result.children.sort(sortByName);

                    result.children.forEach(function(vnfr) {
                        vnfr.children.sort(sortByName);
                    });

                    resolve({
                        statusCode: 200,
                        data: result
                    });
                }).catch(function(error) {
                    // Todo: Can this be made better?
                    // Right now if one of the southbound APIs fails - we just return what's populated so far in result
                    console.log('Problem with ComputeTopology.get vnfr-catalog/vnfr/:id', error, 'Resolving with partial data', result);
                    resolve({
                        statusCode: 200,
                        data: result
                    });
                });
            } catch (e) {
                // API came back with empty ns-instance-opdata response for NSR ID
                // bail
                console.log('Error iterating through ns-instance-opdata response for NSR ID', nsr_id, 'Exception:', e);
                resolve({
                    statusCode: 200,
                    data: result
                })
            }
        }, function(error) {
            // failed to get NSR data.
            // bail
            resolve({
                statusCode: 200,
                data: result
            });
        });
    });
};

NetworkTopology.get = function(req) {
    var api_server = req.query["api_server"];
    var uri = utils.confdPort(api_server);
    uri += '/api/operational/network?deep';
    var headers = _.extend({}, constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        request({
            url: uri,
            method: 'GET',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false
        }, function(error, response, body) {
            if (utils.validateResponse('NetworkTopology.get', error, response, body, resolve, reject)) {
                var data = JSON.parse(response.body);
                var returnData = transforms.transformNetworkTopology(
                    data["ietf-network:network"]
                );
                resolve({
                    statusCode: 200,
                    data: returnData
                });
            };
        });
    })
}

VDUR.get = function(req) {
    var api_server = req.query["api_server"];
    var vnfrID = req.params.vnfr_id;
    var vdurID = req.params.vdur_id;
    var uri = utils.confdPort(api_server);
    uri += '/api/operational/vnfr-catalog/vnfr/' + vnfrID + '/vdur/' + vdurID + '?deep';
    var headers = _.extend({}, constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        request({
            url: uri,
            method: 'GET',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('VDUR.get', error, response, body, resolve, reject)) {
                var data = JSON.parse(response.body);
                var returnData = data["vdur:vdur"];
                return resolve(returnData);
            };
        });
    })
}

CloudAccount.get = function(req) {
    var api_server = req.query["api_server"];
    var uri = utils.confdPort(api_server);
    uri += '/api/config/cloud/account?deep';
    var headers = _.extend({}, constants.HTTP_HEADERS.accept.collection, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        request({
            url: uri,
            method: 'GET',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('CloudAccount.get', error, response, body, resolve, reject)) {
                var data = JSON.parse(response.body);
                var returnData = data["collection"]["rw-cloud:account"];
                resolve({
                    statusCode: 200,
                    data: returnData
                });
            };
        });
    });
}

Config.get = function(req) {
    var api_server = req.query["api_server"];
    var uri = utils.confdPort(api_server);
    uri += '/api/config/launchpad-config?deep';
    var headers = _.extend({}, constants.HTTP_HEADERS.accept.data, {
        'Authorization': req.get('Authorization')
    });
    return new Promise(function(resolve, reject) {
        request({
            url: uri,
            method: 'GET',
            headers: headers,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('Config.get', error, response, body, resolve, reject)) {
                var data = JSON.parse(response.body);
                var returnData = data["rw-launchpad:launchpad-config"];
                resolve({
                    statusCode: 200,
                    data: returnData
                });
            };
        });
    })
}

// Config-Agent Account APIs
ConfigAgentAccount.get = function(req) {
    var self = this;

    var api_server = req.query["api_server"];
    var id = req.params.id;

    if (!id) {
        // Get all config accounts
        return new Promise(function(resolve, reject) {

            var requestHeaders = {};
            _.extend(requestHeaders,
                constants.HTTP_HEADERS.accept.collection, {
                    'Authorization': req.get('Authorization')
                });

            request({
                    url: utils.confdPort(api_server) + '/api/config/config-agent/account',
                    type: 'GET',
                    headers: requestHeaders,
                    forever: constants.FOREVER_ON,
                    rejectUnauthorized: false,
                },
                function(error, response, body) {
                    var data;
                    var statusCode;
                    if (utils.validateResponse('ConfigAgentAccount.get', error, response, body, resolve, reject)) {
                        try {
                            data = JSON.parse(response.body).collection['rw-config-agent:account'];
                            statusCode = response.statusCode;
                        } catch (e) {
                            console.log('Problem with "ConfigAgentAccount.get"', e);
                            var err = {};
                            err.statusCode = 500;
                            err.errorMessage = {
                                error: 'Problem with "ConfigAgentAccount.get": ' + e.toString()
                            }
                            return reject(err);
                        }

                        return resolve({
                            statusCode: statusCode,
                            data: data
                        });
                    };
                });
        });
    } else {
        //Get a specific config account
        return new Promise(function(resolve, reject) {
            var requestHeaders = {};
            _.extend(requestHeaders,
                constants.HTTP_HEADERS.accept.data, {
                    'Authorization': req.get('Authorization')
                });

            request({
                    url: utils.confdPort(api_server) + '/api/config/config-agent/account/' + id,
                    type: 'GET',
                    headers: requestHeaders,
                    forever: constants.FOREVER_ON,
                    rejectUnauthorized: false,
                },
                function(error, response, body) {
                    var data;
                    var statusCode;
                    if (utils.validateResponse('ConfigAgentAccount.get', error, response, body, resolve, reject)) {
                        try {
                            data = JSON.parse(response.body)['rw-config-agent:account'];
                            statusCode = response.statusCode;
                        } catch (e) {
                            console.log('Problem with "ConfigAgentAccount.get"', e);
                            var err = {};
                            err.statusCode = 500;
                            err.errorMessage = {
                                error: 'Problem with "ConfigAgentAccount.get": ' + e.toString()
                            }
                            return reject(err);
                        }

                        return resolve({
                            statusCode: statusCode,
                            data: data
                        });
                    }
                });
        });
    }
};

ConfigAgentAccount.create = function(req) {

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
            url: utils.confdPort(api_server) + '/api/config/config-agent',
            method: 'POST',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData,
        }, function(error, response, body) {
            if (utils.validateResponse('ConfigAgentAccount.create', error, response, body, resolve, reject)) {
                return resolve({
                    statusCode: response.statusCode,
                    data: JSON.stringify(response.body),
                    body:response.body.body
                });
            };
        });
    });
};

ConfigAgentAccount.update = function(req) {

    var api_server = req.query["api_server"];
    var id = req.params.id;
    var data = req.body;

    return new Promise(function(resolve, reject) {
        var jsonData = {
            "rw-config-agent:account": data
        };

        console.log('Updating config-agent', id, ' with', JSON.stringify(jsonData));

        var requestHeaders = {};
        _.extend(requestHeaders,
            constants.HTTP_HEADERS.accept.data,
            constants.HTTP_HEADERS.content_type.data, {
                'Authorization': req.get('Authorization')
            });

        request({
            url: utils.confdPort(api_server) + '/api/config/config-agent/account/' + id,
            method: 'PUT',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
            json: jsonData,
        }, function(error, response, body) {
            if (utils.validateResponse('ConfigAgentAccount.update', error, response, body, resolve, reject)) {
                return resolve({
                    statusCode: response.statusCode,
                    data: JSON.stringify(response.body)
                });
            };
        });
    });
};

ConfigAgentAccount.delete = function(req) {

    var api_server = req.query["api_server"];
    var id = req.params.id;

    if (!id || !api_server) {
        return new Promise(function(resolve, reject) {
            console.log('Must specifiy api_server and id to delete config-agent account');
            return reject({
                statusCode: 500,
                errorMessage: {
                    error: 'Must specifiy api_server and id to delete config agent account'
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
            url: utils.confdPort(api_server) + '/api/config/config-agent/account/' + id,
            method: 'DELETE',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('ConfigAgentAccount.delete', error, response, body, resolve, reject)) {
                return resolve({
                    statusCode: response.statusCode,
                    data: JSON.stringify(response.body)
                });
            };
        });
    });
};


DataCenters.get = function(req) {
    var api_server = req.query["api_server"];
    return new Promise(function(resolve, reject) {
        var requestHeaders = {};
        _.extend(requestHeaders,
            constants.HTTP_HEADERS.accept.data, {
                'Authorization': req.get('Authorization')
            });
        request({
            url: utils.confdPort(api_server) + '/api/operational/datacenters/cloud-accounts?deep',
            method: 'GET',
            headers: requestHeaders,
            forever: constants.FOREVER_ON,
            rejectUnauthorized: false,
        }, function(error, response, body) {
            if (utils.validateResponse('DataCenters.get', error, response, body, resolve, reject)) {
                var returnData = {};
                try {
                    data = JSON.parse(response.body)['rw-launchpad:cloud-accounts'];
                    data.map(function(c) {
                        returnData[c.name] = c.datacenters;
                    })
                    statusCode = response.statusCode;
                } catch (e) {
                    console.log('Problem with "DataCenters.get"', e);
                    var err = {};
                    err.statusCode = 500;
                    err.errorMessage = {
                        error: 'Problem with "DataCenters.get": ' + e.toString()
                    }
                    return reject(err);
                }
                return resolve({
                    statusCode: response.statusCode,
                    data: returnData
                });
            };
        });
    });
}

function sortByName(a, b) {
    return a.name > b.name;
}

module.exports.catalog = Catalog;
module.exports.nsr = NSR;
module.exports.vnfr = VNFR;
module.exports.rift = RIFT;
module.exports.computeTopology = ComputeTopology;
module.exports.networkTopology = NetworkTopology;
module.exports.config = Config;
module.exports.cloud_account = CloudAccount;
module.exports['config-agent-account'] = ConfigAgentAccount;
module.exports.rpc = RPC;
module.exports.data_centers = DataCenters;
