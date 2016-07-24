
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import Alt from '../alt';
import SdnAccountSource from './sdnAccountSource';
import SdnAccountActions from './sdnAccountActions';
import AppHeaderActions from 'widgets/header/headerActions.js';

class createSdnAccountStore {
    constructor() {
        this.exportAsync(SdnAccountSource);
        this.bindAction(SdnAccountActions.CREATE_SUCCESS, this.createSuccess);
        this.bindAction(SdnAccountActions.CREATE_LOADING, this.createLoading);
        this.bindAction(SdnAccountActions.CREATE_FAIL, this.createFail);
        this.bindAction(SdnAccountActions.UPDATE_SUCCESS, this.updateSuccess);
        this.bindAction(SdnAccountActions.UPDATE_LOADING, this.updateLoading);
        this.bindAction(SdnAccountActions.UPDATE_FAIL, this.updateFail);
        this.bindAction(SdnAccountActions.DELETE_SUCCESS, this.deleteSuccess);
        this.bindAction(SdnAccountActions.DELETE_FAIL, this.deleteFail);
        this.bindAction(SdnAccountActions.GET_SDN_ACCOUNT_SUCCESS, this.getSdnAccountSuccess);
        this.bindAction(SdnAccountActions.GET_SDN_ACCOUNT_FAIL, this.getSdnAccountFail);
        this.bindAction(SdnAccountActions.GET_SDN_ACCOUNTS_SUCCESS, this.getSdnAccountsSuccess);
        // this.bindAction(SdnAccountActions.GET_SDN_ACCOUNTS_FAIL, this.getSdnAccountsFail);
        this.exportPublicMethods({
          resetState: this.resetState.bind(this),
          updateSdnAccount: this.updateSdnAccount.bind(this)
        });
        console.log('store constructor');
        this.sdnAccount = {};
        this.sdnAccounts = [];
        this.descriptorCount = 0;
        this.isLoading = true;
        this.params = {
            "odl": [{
                label: "Username",
                ref: 'username'
            }, {
                label: "Password",
                ref: 'password'
            }, {
                label: "URL",
                ref: 'url'
            }],
            // "sdnsim": [{
            //     label: "Username",
            //     ref: "username"
            // }],
            // "mock":[
            //     {
            //         label: "Username",
            //         ref: "username"
            //     }
            // ]
        };
        this.accountType = [{
            "name": "ODL",
            "account-type": "odl",
        }];
        // }, {
        //     "name": "Mock",
        //     "account-type": "mock",
        // }, {
        //     "name": "SdnSim",
        //     "account-type": "sdnsim"
        // }];

        this.sdnAccount = {
            name: '',
            'account-type': 'odl',
            params: this.params['odl']
        };
    }

    createSuccess = () => {
        console.log('successfully created SDN account. API response:', arguments);
        this.setState({
            isLoading: false
        });
    }

    createLoading = () => {
        console.log('sent request to create SDN account. Arguments:', arguments);
        this.setState({
            isLoading: true
        });
    }

    createFail = () => {
        let xhr = arguments[0];
        this.setState({
            isLoading: false
        });
    }

    updateSuccess = () => {
        console.log('successfully updated SDN account. API response:', arguments);
        this.setState({
            isLoading: false
        });
    }

    updateLoading = () => {
        console.log('sent request to update SDN account. Arguments:', arguments);
        this.setState({
            isLoading: true
        });
    }

    updateFail = () => {
        let xhr = arguments[0];
        this.setState({
            isLoading: false
        });
    }

    deleteSuccess = (data) => {
        console.log('successfully deleted SDN account. API response:', arguments);
        this.setState({
            isLoading: false
        });
        if(data.cb) {
            data.cb();
        }
    }

    deleteFail = (data) => {
        let xhr = arguments[0];
        this.setState({
            isLoading: false
        });
    }

    getSdnAccountSuccess = (data) => {
        let sdnAccount = data.sdnAccount;
        sdnAccount.params = sdnAccount[sdnAccount['account-type']];
        let stateObject = {
            sdnAccount: sdnAccount,
            isLoading: false
        }
        this.setState(stateObject);
    }

    getSdnAccountFail = function(data) {
        this.setState({
            isLoading:false
        });
    }

    getSdnAccountsSuccess = (sdnAccounts) => {
        this.setState({
            sdnAccounts: sdnAccounts || [],
            isLoading: false
        })
    }

    resetState = () => {
        this.setState({
            sdnAccount: {
                name: '',
                'account-type': 'odl',
                params: this.params['odl']
            }
        })
    }

    updateSdnAccount = (sdn) => {
        this.setState({
            sdnAccount: sdn
        });
    }
}

export default Alt.createStore(createSdnAccountStore);;
