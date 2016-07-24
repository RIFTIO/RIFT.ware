/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import './about.scss';
import UpTime from 'widgets/uptime/uptime.jsx';
import AppHeader from 'widgets/header/header.jsx';
var aboutActions = require('./aboutActions.js');
var aboutStore = require('./aboutStore.js');
//var MissionControlStore = require('./missionControlStore.js');
class About extends React.Component {
  constructor(props) {
    super(props)
    var self = this;
    this.state = aboutStore.getState();
    aboutStore.listen(this.listenerUpdate);
    aboutStore.get();
    setTimeout(function(){
      if (window.location.hash.split('/').pop() == 'about') {
        aboutStore.createTime();
      }
    }, 200)
  }
  componentWillUnmount() {
    aboutStore.listen(this.listenerUpdate);
  }
  listenerUpdate = (data) => {
      if (data.aboutList) {
        this.setState({
          list:data.aboutList
        })
      }
      if (data.createTime) {
        this.setState({
          createTime:data.createTime
        })
      }
      if (data.descriptorCount) {
        this.setState({
          descriptorCount: data.descriptorCount
        });
      }
    }
  render() {
    let self = this;
    let refPage = window.sessionStorage.getItem('refPage') || '{}';
    refPage = JSON.parse(refPage);

    let mgmtDomainName = window.location.hash.split('/')[2];
    // If in the mission control, create an uptime table;
    var uptime = null;

    var vcs_info = [];

    for (let i = 0; this.state && this.state.list && i < this.state.list.vcs.components.component_info.length; i++) {
      var node = this.state.list.vcs.components.component_info[i];
      vcs_info.push(
          <tr key={i}>
            <td>
              {node.component_name}
            </td>
            <td>
              {node.component_type}
            </td>
            <td>
              {node.state}
            </td>
          </tr>

        )
    }

    if (this.state != null) {
      var html = (
              <div className="table-container-wrapper">
                {uptime}
                <div className="table-container">
                  <h2> Version Info </h2>
                  <table>
                    <thead>
                      <tr>
                        <th>
                          Build SHA
                        </th>
                        <th>
                          Version
                        </th>
                        <th>
                          Build Date
                        </th>
                      </tr>
                    </thead>
                    <tbody>
                      <tr>
                        <td>
                          {this.state.list ? this.state.list.version.build_sha : null}
                        </td>
                        <td>
                          {this.state.list ? this.state.list.version.version : null}
                        </td>
                        <td>
                          {this.state.list ? this.state.list.version.build_date : null}
                        </td>
                      </tr>
                    </tbody>
                  </table>
                </div>
                <div className="table-container">
                  <h2> Component Info </h2>
                  <table>
                    <thead>
                      <tr>
                        <th>
                          Component Name
                        </th>
                        <th>
                          Component Type
                        </th>
                        <th>
                          State
                        </th>
                      </tr>
                    </thead>
                    <tbody>
                      {vcs_info}
                    </tbody>
                  </table>
                </div>
              </div>
              );
    } else {
      html = ''
    }
    return (
            <div className="about-container">
              {html}
            </div>
            )
  }
}
export default About;
