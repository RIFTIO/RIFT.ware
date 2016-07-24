
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from 'widgets/skyquake_container/skyquakeAltInstance';
var crashDetailsActions = Alt.generateActions(
                                                'getCrashDetailsSuccess',
                                                'getCrashDetailsLoading',
                                                'getCrashDetailsFail',
                                                );

module.exports = crashDetailsActions;
