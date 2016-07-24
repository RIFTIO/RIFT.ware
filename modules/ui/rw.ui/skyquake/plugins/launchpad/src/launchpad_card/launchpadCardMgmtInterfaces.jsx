
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';

export default class managementInterfaces extends React.Component {
  constructor(props) {
    super(props);
  }
  componentWillReceiveProps(nextProps) {

  }
  render() {
    let html;
    let isDisplayed = this.props.display;
    let status;
    let applicationDashboards = this.props.interfaces.sort(function(a,b) {
      try {
            if ((a["short-name"] + '-' + a.id.substr(0,4)) > (b["short-name"] + '-' + b.id.substr(0,4))) {
              return 1;
            }
      } catch(e) {
        return 1;
      }
      return -1;
    });
    if(applicationDashboards.length > 0){
      status = applicationDashboards.map(function(i, index) {
        let mgmtLink = i["dashboard-url"] ? i["dashboard-url"] : 'None';
          return (
            <li key={index}><h3>{i["short-name"] + '-' + i.id.substr(0,4)}</h3><a href={i["dashboard-url"]} target="_blank">{mgmtLink}</a></li>
          )
        });
    } else {
      status = <li>No Application Dashboard Links have been specified.</li>
    }
    html = (
          <ul>
            {status}
          </ul>
      )
    return (<div className={this.props.className + (isDisplayed ? '_open':'_close')}><h2>Application Dashboard Links</h2>{html}</div>);
  }
}

managementInterfaces.defaultProps = {
  display: false,
  interfaces: []
}


