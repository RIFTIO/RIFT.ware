
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';
var createCloudAccountActions = Alt.generateActions(
                                                'createSuccess',
                                                'createLoading',
                                                'createFail',
                                                'updateSuccess',
                                                'updateLoading',
                                                'updateFail',
                                                'deleteSuccess',
                                                'deleteCloudAccountFail',
                                                'getCloudAccountSuccess',
                                                'getCloudAccountsSuccess',
                                                'getCloudAccountsFail',
                                                'getCloudAccountFail',
                                                'validateError',
                                                'validateReset'
                                                );

module.exports = createCloudAccountActions;
