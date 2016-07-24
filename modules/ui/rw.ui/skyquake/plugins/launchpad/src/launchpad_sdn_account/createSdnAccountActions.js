
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from 'widgets/skyquake_container/skyquakeAltInstance';
var createSdnAccountActions = Alt.generateActions(
                                                'createSuccess',
                                                'createLoading',
                                                'createFail',
                                                'updateSuccess',
                                                'updateLoading',
                                                'updateFail',
                                                'deleteSuccess',
                                                'deleteFail',
                                                'getSdnAccountSuccess',
                                                'getSdnAccountFail',
                                                'validateError',
                                                'validateReset',
                                                'getSdnAccountsSuccess'
                                                );

module.exports = createSdnAccountActions;
