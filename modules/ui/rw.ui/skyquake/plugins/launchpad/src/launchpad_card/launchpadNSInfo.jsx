
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
//THIS SHOULD BE REMOVED ONCE EPA PARAMS ARE MOVED OVER

import React from 'react';
let ReactCSSTransitionGroup = require('react-addons-css-transition-group');
class LaunchpadNSInfo extends React.Component {
  constructor(props) {
    super(props);
    // this.state = {metrics: props.data};
  }
  render() {
    let metricSet = [];
    let toDisplay;
    this.props.data.map((metric, index)=> {
      if (metric.hasOwnProperty('label') && this.props.name != "NFVI-METRICS") {

      let displayValue = '';
      if (metric.hasOwnProperty('vm')) {
        displayValue = metric.vm + ' ' + metric.unit;
      }
      if (metric.hasOwnProperty('active-vm')) {
        displayValue = metric["active-vm"] + ' of ' + (parseInt(metric["active-vm"]) + parseInt(metric["inactive-vm"])) + ' Active';
      }
      if (metric.hasOwnProperty('used')) {
        displayValue = metric.used.value + ' / ' + metric.total.value + metric.total.unit;
      }

         metricSet.push((
            <li key={index}>
              <dl>
                <dt>{metric.label}:</dt>
                <dd>{displayValue}</dd>
              </dl>
            </li>
          ))
      };
      if(this.props.name == "NFVI-METRICS") {
        toDisplay = <NfviMetricBars metrics={this.props.data} />
      }
    });
        var infoClass = {
      'EPA-PARAM' : {
        height: '300px',
        overflow: 'scroll',
        width: '707px',
      },
      'NFVI-METRICS' : {
        height: '400px'
      }
    }
    // style={infoClass[this.props.name]}
    if (!metricSet.length) {
      let msgString = ''

      //DEMO ONLY
      if(this.props.name == "NFVI-METRICS") {
        toDisplay = <NfviMetricBars metrics={this.props.data} />
      } else {
        toDisplay = (<div className="empty">{ msgString }</div>);
      }
    } else {
      toDisplay = (
        <ul >
          {
            metricSet
          }
        </ul>
      )
    }

    return (
      <div style={{overflow: 'hidden'}}>
        <div className="launchpadCard_title">
          { this.props.name }
        </div>
        <div className={"launchpadCard_data-list" + ' ' + this.props.name}>
          { toDisplay }
        </div>
      </div>
    )
  }
}
LaunchpadNSInfo.propTypes = {
  data: React.PropTypes.array,
  name: React.PropTypes.string
 };
LaunchpadNSInfo.defaultProps = {
  name: 'Loading...',
  data: []
};

export default LaunchpadNSInfo;
