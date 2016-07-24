
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';
var createConfigAgentAccountActions = Alt.generateActions(
                                                'createConfigAccountSuccess',
                                                'createConfigAccountLoading',
                                                'createConfigAccountFailed',
                                                'updateSuccess',
                                                'updateLoading',
                                                'updateFail',
                                                'deleteSuccess',
                                                'deleteFail',
                                                'getConfigAgentAccountSuccess',
                                                'getConfigAgentAccountsSuccess',
                                                'getConfigAgentAccountsFail',
                                                'getConfigAgentAccountFail',
                                                'validateError',
                                                'validateReset'
                                                );

module.exports = createConfigAgentAccountActions;
