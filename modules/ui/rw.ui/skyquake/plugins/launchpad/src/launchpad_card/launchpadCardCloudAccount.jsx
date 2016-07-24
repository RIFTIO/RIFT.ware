
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

import React from 'react';
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';

export default class LaunchpadCardCloudAccount extends React.Component {
  constructor(props) {
    super(props);
  }
  componentWillReceiveProps(nextProps) {

  }
  render() {
    let html;
    let isDisplayed = this.props.display;
    let status = [];
    if (this.props.nsr['cloud-account']) {
      status.push(
        (<li key="nsr"><h3>NSR: {this.props.nsr['cloud-account']}</h3></li>)
      )
    }
    html = (
          <ul>
            {status}
          </ul>
      )
    return (<div className={this.props.className + (isDisplayed ? '_open':'_close')}><h2>Cloud Accounts</h2>{html}</div>);
  }
}

LaunchpadCardCloudAccount.defaultProps = {
  display: false,
  nsr: {}
}


