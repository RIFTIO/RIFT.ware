/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import './sdnAccount.scss';
import Button from 'widgets/button/rw.button.js';
import _ from 'lodash';
import Crouton from 'react-crouton';
import 'style/common.scss';
class SdnAccount extends React.Component {
    constructor(props) {
        super(props);
        this.state = this.props.store.getState();
        this.props.store.listen(this.storeListener);
        this.state.actions = this.props.actions;
        this.state.store = this.props.store;
    }
    storeListener = (state) => {
        this.setState(
            state
        )
    }
    componentWillUnmount() {
        this.state.store.unlisten(this.storeListener);
    }
    create(e) {
        e.preventDefault();
        var self = this;
        if (self.state.sdnAccount.name == "") {
            self.context.flux.actions.global.showError("Please give the SDN account a name");
            return;
        } else {
            var type = self.state.sdnAccount['account-type'];
            if (typeof(self.state.params[type]) != 'undefined') {
                var params = self.state.params[type];
                for (var i = 0; i < params.length; i++) {
                    var param = params[i].ref;
                    if (typeof(self.state.sdnAccount[type]) == 'undefined' || typeof(self.state.sdnAccount[type][param]) == 'undefined' || self.state.sdnAccount[type][param] == "") {
                        if (!params[i].optional) {
                            self.context.flux.actions.global.showError("Please fill all account details");
                            return;
                        }
                    }
                }
            }
        }
        self.state.actions.validateReset();

        let sdnAccountData = _.cloneDeep(self.state.sdnAccount);
        delete sdnAccountData.params;

        this.state.store.create(sdnAccountData).then(function() {
            self.context.router.push({pathname:'accounts'});
        },
         function() {
            self.context.flux.actions.global.showError("There was an error creating your SDN Account. Please contact your system administrator.")
         });
    }
    update(e) {
        e.preventDefault();
        var self = this;

        if (self.state.sdnAccount.name == "") {
            self.context.flux.actions.global.showError("Please give the SDN account a name");
            return;
        }

          var submit_obj = {
            'account-type':self.state.sdnAccount['account-type'],
            name:self.state.sdnAccount.name
          }
          submit_obj[submit_obj['account-type']] = self.state.sdnAccount[submit_obj['account-type']];
          if(!submit_obj[submit_obj['account-type']]) {
            submit_obj[submit_obj['account-type']] = {};
          }
          for (var key in self.state.sdnAccount.params) {
            if (submit_obj[submit_obj['account-type']][key]) {
                //submit_obj[submit_obj['account-type']][key] = self.state.sdnAccount.params[key];
            } else {
                submit_obj[submit_obj['account-type']][key] = self.state.sdnAccount.params[key];
            }
          }

         this.state.store.update(submit_obj).then(function() {
            self.context.router.push({pathname:'accounts'});
         },
         function() {
            self.context.flux.actions.global.showError("There was an error updating your SDN Account. Please contact your system administrator.")
         });
    }
    cancel = (e) => {
        e.preventDefault();
        e.stopPropagation();
        this.context.router.push({pathname:'accounts'});
    }
    componentWillReceiveProps(nextProps) {}
    shouldComponentUpdate(nextProps) {
        return true;
    }
    handleDelete = () => {
        let self = this;
        this.state.store.delete(self.state.sdnAccount.name, function() {
            self.context.router.push({pathname:'accounts'});
        });
    }
    handleNameChange(event) {
        var temp = this.state.sdnAccount;
        temp.name = event.target.value;
        this.setState({
            sdnAccount: temp
        });
    }
    handleAccountChange(node, event) {
        var temp = this.state.sdnAccount;
        temp['account-type'] = event.target.value;
        this.setState({
            sdnAccount: temp
        });
    }
    handleParamChange(node, event) {
        var temp = this.state.sdnAccount;
        if (!this.state.sdnAccount[this.state.sdnAccount['account-type']]) {
            temp[temp['account-type']] = {}
        }
        temp[temp['account-type']][node.ref] = event.target.value;
        temp.params[node.ref] = event.target.value;
        this.state.store.updateSdnAccount(temp);
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
            name = <label>{this.state.sdnAccount.name}</label>
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
        if (this.state.params[this.state.sdnAccount['account-type']]) {
            var paramsStack = [];
            var optionalField = '';
            for (var i = 0; i < this.state.params[this.state.sdnAccount['account-type']].length; i++) {
                var node = this.state.params[this.state.sdnAccount['account-type']][i];
                var value = ""
                if (this.state.sdnAccount[this.state.sdnAccount['account-type']]) {
                    value = this.state.sdnAccount[this.state.sdnAccount['account-type']][node.ref]
                }
                if (this.props.edit && this.state.sdnAccount.params) {
                    value = this.state.sdnAccount.params[node.ref];
                }

                // If you're on the edit page, but the params have not been recieved yet, this stops us from defining a default value that is empty.
                if (this.props.edit && !this.state.sdnAccount.params) {
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
            name = <label>{this.state.sdnAccount.name}</label>
            var buttons = [
                <a key="0" onClick={this.handleDelete} className="delete">Remove Account</a>,
                    <a key="1" onClick={this.cancel} className="cancel">Cancel</a>,
                    <Button key="2" role="button" onClick={this.update.bind(this)} className="update" label="Update" />
            ]
        }

        var html = (

              <form className="app-body create sdnAccount"  onSubmit={this.preventDefault} onKeyDown={this.evaluateSubmit}>
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
SdnAccount.contextTypes = {
    router: React.PropTypes.object,
    flux: React.PropTypes.object
};
export default SdnAccount
