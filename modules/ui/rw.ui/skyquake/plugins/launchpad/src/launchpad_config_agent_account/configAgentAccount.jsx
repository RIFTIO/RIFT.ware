/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import './configAgentAccount.scss';
import Button from 'widgets/button/rw.button.js';
import _ from 'lodash';
import Crouton from 'react-crouton';
import 'style/common.scss';
class ConfigAgentAccount extends React.Component {
    constructor(props) {
        super(props);
        this.state = this.props.store.getState();
        this.props.store.listen(this.storeListener);
        this.state.actions = this.props.actions;
        this.state.store = this.props.store;


        this.state.validateErrorEvent = 0;
        this.state.validateErrorMsg = '';

    }
    storeListener = (state) => {
        this.setState(
            state
        )
    }
    create(e) {
        e.preventDefault();
        var self = this;
        if (self.state.configAgentAccount.name == "") {
            self.context.flux.actions.global.showError("Please give the config agent account a name");
            return;
        } else {
            var type = self.state.configAgentAccount['account-type'];
            if (typeof(self.state.params[type]) != 'undefined') {
                var params = self.state.params[type];
                for (var i = 0; i < params.length; i++) {
                    var param = params[i].ref;
                    if (typeof(self.state.configAgentAccount[type]) == 'undefined' || typeof(self.state.configAgentAccount[type][param]) == 'undefined' || self.state.configAgentAccount[type][param] == "") {
                        if (!params[i].optional) {
                            self.context.flux.actions.global.showError("Please fill all account details");
                            return;
                        }
                    }
                }
            }
        }

        let configAgentAccountData = _.cloneDeep(self.state.configAgentAccount);
        delete configAgentAccountData.params;

        this.state.store.create(configAgentAccountData).then(function() {
            self.context.router.push({pathname:'accounts'});
        }, function() {
            self.context.flux.actions.global.showError("There was an error creating your Config Agent Account. Please contact your system administrator")
        });
    }
    update(e) {
        e.preventDefault();
        var self = this;

        if (self.state.configAgentAccount.name == "") {
            self.context.flux.actions.global.showError("Please give the config agent account a name");
            return;
        }

          var submit_obj = {
            'account-type':self.state.configAgentAccount['account-type'],
            name:self.state.configAgentAccount.name
          }
          submit_obj[submit_obj['account-type']] = self.state.configAgentAccount[submit_obj['account-type']];
          if(!submit_obj[submit_obj['account-type']]) {
            submit_obj[submit_obj['account-type']] = {};
          }
          for (var key in self.state.configAgentAccount.params) {
            if (submit_obj[submit_obj['account-type']][key]) {
                //submit_obj[submit_obj['account-type']][key] = self.state.configAgentAccount.params[key];
            } else {
                if(submit_obj[submit_obj['account-type']][key] == "") {
                    self.context.flux.actions.global.showError("Please fill all account details");
                    return;
                } else {
                    submit_obj[submit_obj['account-type']][key] = self.state.configAgentAccount.params[key];
                }
            }
          }

         this.state.store.update(submit_obj).then(function() {
            self.context.router.push({pathname:'accounts'});
         }, function() {
            self.context.flux.actions.global.showError("There was an error updating your Config Agent Account. Please contact your system administrator")
        });
    }
    cancel = (e) => {
        e.preventDefault();
        e.stopPropagation();
        this.context.router.push({pathname:'accounts'});
    }
    preventDefault = (e) => {
        e.preventDefault();
        e.stopPropagation();
    }
    evaluateSubmit = (e) => {
        if (e.keyCode == 13) {
            if (this.props.edit) {
                this.update(e);
            } else {
                this.create(e);
            }
            e.preventDefault();
            e.stopPropagation();
        }
    }
    componentWillReceiveProps(nextProps) {
    }
    componentWillMount() {
        this.props.store.resetState();
    }
    componentWillUnmount() {
        this.state.store.unlisten(this.storeListener);
    }
    shouldComponentUpdate(nextProps) {
        return true;
    }
    handleDelete = () => {
        let self = this;
        this.state.store.delete(self.state.configAgentAccount.name).then(function() {
            self.context.router.push({pathname:'accounts'}, function() {
            self.context.flux.actions.global.showError("There was an error deleting your Config Agent Account. Please contact your system administrator")
        });
        });
    }
    handleNameChange(event) {
        var temp = this.state.configAgentAccount;
        temp.name = event.target.value;
        this.setState({
            configAgentAccount: temp
        });
    }
    handleAccountChange(node, event) {
        var temp = this.state.configAgentAccount;
        temp['account-type'] = event.target.value;
        this.setState({
            configAgentAccount: temp
        })
    }
    handleParamChange(node, event) {
        var temp = this.state.configAgentAccount;
        if (!this.state.configAgentAccount[this.state.configAgentAccount['account-type']]) {
            temp[temp['account-type']] = {}
        }
        temp[temp['account-type']][node.ref] = event.target.value;
        temp.params[node.ref] = event.target.value;
        this.state.store.updateConfigAgentAccount(temp);
    }
    render() {
        // This section builds elements that only show up on the create page.
        var name = <label>Name <input type="text" onChange={this.handleNameChange.bind(this)} style={{'textAlign':'left'}} /></label>
        let params = null;
        let selectAccount = null;
        var buttons = [
            <Button key="0" onClick={this.cancel} className="cancel light" label="Cancel"></Button>,
            <Button key="1" role="button" onClick={this.create.bind(this)} className="save dark" label="Save" />
        ]
        if (this.props.edit) {
            name = <label>{this.state.configAgentAccount.name}</label>
            var buttons = [
                <a key="0" onClick={this.handleDelete} className="delete">Remove Account</a>,
                    <Button key="1" onClick={this.cancel} className="cancel light" label="Cancel"></Button>,
                    <Button key="2" role="button" onClick={this.update.bind(this)} className="update dark" label="Update" />
            ]
        }
        // This creates the create screen radio button for account type.
        var selectAccountStack = [];
        if (!this.props.edit) {
            for (var i = 0; i < this.state.accountType.length; i++) {
                var node = this.state.accountType[i];
                selectAccountStack.push(
                  <label key={i}>
                    <input type="radio" name="account" onChange={this.handleAccountChange.bind(this, node)} defaultChecked={node.name == this.state.accountType[0].name} value={node['account-type']} /> {node.name}
                  </label>
                )

            }
            selectAccount = (
                <div className="select-type" style={{"marginLeft":"27px"}}>
                    Select Account Type:
                    {selectAccountStack}
                </div>
            )
        }



        // This sections builds the parameters for the account details.
        if (this.state.params[this.state.configAgentAccount['account-type']]) {
            var paramsStack = [];
            var optionalField = '';
            for (var i = 0; i < this.state.params[this.state.configAgentAccount['account-type']].length; i++) {
                var node = this.state.params[this.state.configAgentAccount['account-type']][i];
                var value = ""
                if (this.state.configAgentAccount[this.state.configAgentAccount['account-type']]) {
                    value = this.state.configAgentAccount[this.state.configAgentAccount['account-type']][node.ref]
                }
                if (this.props.edit && this.state.configAgentAccount.params) {
                    value = this.state.configAgentAccount.params[node.ref];
                }

                // If you're on the edit page, but the params have not been recieved yet, this stops us from defining a default value that is empty.
                if (this.props.edit && !this.state.configAgentAccount.params) {
                    break;
                }
                if (node.optional) {
                    optionalField = <span className="optional">Optional</span>;
                }
                paramsStack.push(
                    <label key={node.label}>
                      <label className="create-fleet-pool-params">{node.label} {optionalField}</label>
                      <input className="create-fleet-pool-input" type="text" onChange={this.handleParamChange.bind(this, node)} value={value}/>
                    </label>
                );
            }

            params = (
                <li className="create-fleet-pool">
              <h3> Enter Account Details</h3>
              {paramsStack}
          </li>
            )
        } else {
            params = (
                <li className="create-fleet-pool">
                    <h3> Enter Account Details</h3>
                    <label style={{'marginLeft':'17px', color:'#888'}}>No Details Required</label>
                </li>
            )
        }

        // This section builds elements that only show up in the edit page.
        if (this.props.edit) {
            name = <label>{this.state.configAgentAccount.name}</label>
            var buttons = [
                <a key="0" onClick={this.handleDelete} className="delete">Remove Account</a>,
                    <a key="1" onClick={this.cancel} className="cancel">Cancel</a>,
                    <Button key="2" role="button" onClick={this.update.bind(this)} className="update" label="Update" />
            ]
        }

        var html = (

              <form className="app-body create configAgentAccount"  onSubmit={this.preventDefault} onKeyDown={this.evaluateSubmit}>
                  <h2 className="create-management-domain-header name-input">
                       {name}
                  </h2>
                  {selectAccount}
                  <ol className="flex-row">
                      {params}
                  </ol>
                  <div className="form-actions">
                      {buttons}
                  </div>
              </form>
        )
        return html;
    }
}
ConfigAgentAccount.contextTypes = {
    router: React.PropTypes.object,
    flux: React.PropTypes.object
};
export default ConfigAgentAccount
