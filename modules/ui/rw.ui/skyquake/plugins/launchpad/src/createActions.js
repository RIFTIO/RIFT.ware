
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
var alt = require('../core/alt');
module.exports = alt.generateActions(
                                       'getNetworkServicesSuccess',
                                       'getNetworkServicesError',
                                       'createEnvironmentSuccess',
                                       'createEnvironmentError',
                                       'getPoolsSuccess',
                                       'getPoolsError',
                                       'getSlaParamsSuccess',
                                       'getSlaParamsError',
                                       'validateError',
                                       'validateReset'
                                       );
