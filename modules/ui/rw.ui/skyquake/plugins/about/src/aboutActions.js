
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from 'widgets/skyquake_container/skyquakeAltInstance';
var aboutActions = Alt.generateActions(
                                                'getAboutSuccess',
                                                'getAboutLoading',
                                                'getAboutFail',
                                                'getCreateTimeSuccess',
                                                'getCreateTimeLoading',
                                                'getCreateTimeFail'
                                                );

module.exports = aboutActions;
