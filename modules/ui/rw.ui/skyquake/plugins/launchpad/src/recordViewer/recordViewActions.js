
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';

export default Alt.generateActions(
                                   'getNSRSuccess','getNSRError','getNSRLoading',
                                   'getVNFRSocketLoading','getVNFRSocketError','getVNFRSocketSuccess',
                                   'getNSRSocketSuccess','getNSRSocketError','getNSRSocketLoading',
                                   'getRawSuccess','getRawError','getRawLoading',
                                   'loadRecord',
                                   'constructAndTriggerConfigPrimitive',
                                   'execNsConfigPrimitiveLoading', 'execNsConfigPrimitiveSuccess', 'execNsConfigPrimitiveError',
                                   'createScalingGroupInstanceLoading', 'createScalingGroupInstanceSuccess', 'createScalingGroupInstanceError',
                                   'deleteScalingGroupInstanceLoading', 'deleteScalingGroupInstanceSuccess', 'deleteScalingGroupInstanceError'
                                   );
