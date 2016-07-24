/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import Button from 'widgets/button/rw.button.js';
import './cloudAccount.scss';
import _ from 'lodash';
import Crouton from 'react-crouton';
import 'style/common.scss';
// var CloudAccountStore = require('./cloudAccountStore')
// var CloudAccountActions = require('./cloudAccountActions')
class CloudAccount extends React.Component {
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
        );
    }
    create(e) {
        e.preventDefault();
        var self = this;
        if (self.state.cloud.name == "") {
            self.context.flux.actions.global.showError("Please give the cloud account a name");
            return;
        } else {
            var type = self.state.cloud['account-type'];
            if (typeof(self.state.params[type]) != 'undefined') {
                var params = self.state.params[type];
                for (var i = 0; i < params.length; i++) {
                    var param = params[i].ref;
                    if (typeof(self.state.cloud[type]) == 'undefined' || typeof(self.state.cloud[type][param]) == 'undefined' || self.state.cloud[type][param] == "") {
                        if (!params[i].optional) {
                            self.context.flux.actions.global.showError("Please fill all account details");
                            return;
                        }
                    }
                }
            }
        }
        this.state.actions.validateReset();

        let cloudAccountData = _.cloneDeep(self.state.cloud);
        delete cloudAccountData.params;

        this.state.store.create(cloudAccountData).then(function() {
            self.context.router.push({pathname:'accounts'});
        },
        function() {
            self.context.flux.actions.global.showError("There was an error creating your Cloud Account. Please contact your system administrator")
        });
    }
    update(e) {
        e.preventDefault();
        var self = this;

        if (self.state.cloud.name == "") {
            self.context.flux.actions.global.showError("Please give the cloud account a name");
            return;
        }

          var submit_obj = {
            'account-type':self.state.cloud['account-type'],
            name:self.state.cloud.name
          }
          submit_obj[submit_obj['account-type']] = self.state.cloud[submit_obj['account-type']];
          if(!submit_obj[submit_obj['account-type']]) {
            submit_obj[submit_obj['account-type']] = {};
          }
          for (var key in self.state.cloud.params) {
            if (submit_obj[submit_obj['account-type']][key]) {
                //submit_obj[submit_obj['account-type']][key] = self.state.cloud.params[key];
                console.log('hold')
            } else {
                if(self.state.cloud.params[key] == "") {
                    self.context.flux.actions.global.showError("Please fill all account details");
                    return;
                } else {
                   submit_obj[submit_obj['account-type']][key] = self.state.cloud.params[key];
                }

            }
          }

         this.state.store.update(submit_obj).then(function() {
            // hack to restore MC app URL
            self.context.router.push({pathname:'accounts'});
         }, function() {
                self.context.flux.actions.global.showError("There was an error updating your Cloud Account. Please contact your system administrator");
         }
         );
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
            if (this.props.isEdit) {
                this.update(e);
            } else {
                this.create(e);
            }
            e.stopPropagation();
        }
    }
    componentWillMount() {
        this.props.store.resetState();
    }
    componentWillUnmount() {
        this.props.store.unlisten(this.storeListener);
    }
    componentWillReceiveProps(nextProps) {
    }
    shouldComponentUpdate(nextProps) {
        return true;
    }
    handleDelete = () => {
        let self = this;
        let msg = 'Preparing to delete "' + self.state.cloud.name + '"' +
        ' Are you sure you want to delete this cloud account?"';
        if (window.confirm(msg)) {
            this.state.store.delete(self.state.cloud.name).then(function() {
                self.context.router.push({pathname:'accounts'});
            }, function() {
                self.context.flux.actions.global.showError(
                    'There was an error deleting your Cloud Account. '+
                    'Please contact your system administrator');
            });

        }
    }
    handleNameChange(event) {
        var temp = this.state.cloud;
        temp.name = event.target.value;
        this.setState({
            cloud: temp
        });
    }
    handleAccountChange(node, event) {
        var temp = this.state.cloud;
        temp['account-type'] = event.target.value;
        this.setState({
            cloud: temp
        })
    }
    handleParamChange(node, event) {
            var temp = this.state.cloud;
            if (!this.state.cloud[this.state.cloud['account-type']]) {
                temp[temp['account-type']] = {}
            }
            temp[temp['account-type']][node.ref] = event.target.value;
            temp.params[node.ref] = event.target.value;
            this.state.store.updateCloud(temp);
    }
    handleSelectSdnAccount = (e) => {
        var tmp = this.state.cloud;
        if(e) {
            tmp['sdn-account'] = e;
        } else {
            if(tmp['sdn-account']) {
                delete tmp['sdn-account'];
            }
        }
        this.state.store.updateCloud(tmp);
    }
    render() {
        let selectAccount = null;
        let params = null;
        // This section builds elements that only show up on the create page.
        var name = <label>Name <input type="text" onChange={this.handleNameChange.bind(this)} style={{'textAlign':'left'}} /></label>
        var buttons = [
            <Button key="0" onClick={this.cancel} className="cancel light" label="Cancel"></Button>,
            <Button key="1" role="button" onClick={this.create.bind(this)} className="save dark" label="Save" />
        ]
        if (this.props.isEdit) {
            name = <label>{this.state.cloud.name}</label>
            var buttons = [
                <a  key="1" onClick={this.handleDelete} ng-click="create.delete(create.cloud)" className="delete">Remove Account</a>,
                    <Button key="2" onClick={this.cancel} className="cancel light" label="Cancel"></Button>,,
                    <Button key="3" role="button" onClick={this.update.bind(this)} className="update dark" label="Update" />
            ]

        }
        // This creates the create screen radio button for account type.
        var selectAccountStack = [];
        if (!this.props.isEdit) {
            for (var i = 0; i < this.state.accountType.length; i++) {
                var node = this.state.accountType[i];
                selectAccountStack.push(
                  <label key={node.name}>
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

        // This section builds the sdn account drop down, if available
        let sdnAccounts = null;
        if (this.props.sdnAccounts && !this.props.isEdit) {
            sdnAccounts = (
                <div className="associateSdnAccount">
                    Associate SDN Account
                    <div>
                        <SelectOption  options={this.props.sdnAccounts} onChange={this.handleSelectSdnAccount} />
                    </div>

                </div>
            );
        }
        if (this.state.cloud['sdn-account'] && this.props.isEdit) {
            sdnAccounts = (
                <div className="associateSdnAccount">
                    SDN Account:
                    <div>
                        {this.state.cloud['sdn-account']}
                    </div>
                </div>
           );
        }

        // This sections builds the parameters for the account details.
        if (this.state.params[this.state.cloud['account-type']]) {
            var paramsStack = [];
            var optionalField = '';
            for (var i = 0; i < this.state.params[this.state.cloud['account-type']].length; i++) {
                var node = this.state.params[this.state.cloud['account-type']][i];
                var value = ""
                if (this.state.cloud[this.state.cloud['account-type']]) {
                    value = this.state.cloud[this.state.cloud['account-type']][node.ref]
                }
                if (this.props.isEdit && this.state.cloud.params) {
                    value = this.state.cloud.params[node.ref];
                }

                // If you're on the edit page, but the params have not been recieved yet, this stops us from defining a default value that is empty.
                if (this.props.isEdit && !this.state.cloud.params) {
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
        if (this.props.isEdit) {
            name = <label>{this.state.cloud.name}</label>
            var buttons = [
                <a key="0"  onClick={this.handleDelete} className="delete">Remove Account</a>,
                    <a key="1" onClick={this.cancel} className="cancel">Cancel</a>,
                    <Button key="2" role="button" onClick={this.update.bind(this)} className="update dark" label="Update" type="submit"/>
            ]
        }

        var html = (

              <form className="app-body create cloudAccount" onSubmit={this.preventDefault} onKeyDown={this.evaluateSubmit}>
                  <h2 className="create-management-domain-header name-input">
                       {name}
                  </h2>
                  {selectAccount}
                  {sdnAccounts}
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
CloudAccount.contextTypes = {
    router: React.PropTypes.object,
    flux: React.PropTypes.object
};

export default CloudAccount


class SelectOption extends React.Component {
  constructor(props){
    super(props);
  }
  handleOnChange = (e) => {
    this.props.onChange(JSON.parse(e.target.value));
  }
  render() {
    let html;
    html = (
      <select className={this.props.className} onChange={this.handleOnChange}>
        {
          this.props.options.map(function(op, i) {
            return <option key={i} value={JSON.stringify(op.value)}>{op.label}</option>
          })
        }
      </select>
    );
    return html;
  }
}
SelectOption.defaultProps = {
  options: [],
  onChange: function(e) {
    console.dir(e)
  }
}
