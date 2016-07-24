
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from './alt';
module.exports = Alt.generateActions(
                                       'getNsrInstancesSuccess',
                                       'getNsrInstancesError',
                                       'getLaunchpadConfigSuccess',
                                       'getLaunchpadConfigError',
                                       'openNSRSocketLoading',
                                       'openNSRSocketSuccess',
                                       'openNSRSocketError',
                                       'setNSRStatusSuccess',
                                       'setNSRStatusError',
                                       'deleteNsrInstance',
                                       'deleteNsrInstanceSuccess',
                                       'deleteNsrInstanceError',
                                       'nsrControlSuccess',
                                       'nsrControlError',
                                       'slideNoStateChange',
                                       'slideNoStateChangeSuccess',
                                       'validateError',
                                       'validateReset',
                                       'deletingNSR',
                                       'openNsrCard',
                                       'closeNsrCard',
                                       'instantiateNetworkService',
                                       'setNsListPanelVisible',
                                       );
