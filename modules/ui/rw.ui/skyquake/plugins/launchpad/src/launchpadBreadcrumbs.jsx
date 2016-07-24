/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';

export default class LaunchpadBreadcrumbs extends React.Component {

  constructor(props) {
    super(props);
    this.current = props.current;
  }
  componentDidMount() {

  }
  componentWillReceiveProps(props) {

  }
  breadcrumbItem(url, name, isCurrent) {
    if (isCurrent) {
      return (<span className="current">{name}</span>);
    } else {
      return (<a href={url} onClick={this.props.clickHandler}>{name}</a>);
    }
  }
  render() {
    let mgmtDomainName = window.location.hash.split('/')[2];
    let nsrId = window.location.hash.split('/')[3];
    let url_base = '#/launchpad/' + mgmtDomainName + '/' + nsrId;
    let html = (
      <div className="header-nav">
        <a href={'#/launchpad/' + mgmtDomainName} onClick={this.props.clickHandler}>DASHBOARD</a>
        <span className="spacer"> > </span>
        {this.breadcrumbItem(url_base+'/detail', 'Viewport', (this.props.current == 'viewport'))}
        <span className="spacer"> | </span>
        {this.breadcrumbItem(url_base+'/topology', 'Topology', (this.props.current == 'topology' ))}
        <span className="spacer"> | </span>
        {this.breadcrumbItem(url_base+'/topologyL2', 'Topology L2', (this.props.current == 'topologyL2' ))}
        <span className="spacer"> | </span>
        {this.breadcrumbItem(url_base+'/topologyL2Vm', 'Topology L2Vm', (this.props.current == 'topologyL2Vm'))}
      </div>
    );
    return html;
  }
}
