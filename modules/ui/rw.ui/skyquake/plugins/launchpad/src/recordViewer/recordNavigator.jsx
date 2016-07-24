
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import RecordViewActions from './recordViewActions.js';
import LoadingIndicator from 'widgets/loading-indicator/loadingIndicator.jsx';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import './recordNavigator.scss';
import nsdImg from 'style/img/catalog-default.svg';

export default class RecordNavigator extends React.Component{
  constructor(props) {
    super(props)
  }
  render(){
    let navClass = 'catalogItems';

    let self = this;
    let html;
    let className = this.props.isLoading ? 'loading' : '';
    let nav = [];

    this.props.nav.map(function(n, k) {
      let itemClassName = navClass + '_item';
      let catalog_name = (n.type == 'nsr' ? <span>({n.nsd_name})</span> : '');
      let scalingGroupClass = '';
      let scalingGroupTitleClass = '';
      let scalingGroupTitle = '';
      let navObj = [];
      if (n.scalingGroupName) {
        scalingGroupClass = navClass + ' -is-scaled';
        scalingGroupTitleClass = scalingGroupClass + '_title';
        scalingGroupTitle = n.scalingGroupName + '_' + n.scalingGroupInstanceId;
        n.vnfr && n.vnfr.map((vnfr, vnfrIndex) => {
          let iClassName = itemClassName;
          if(vnfr.id == self.props.activeNavID) {
            iClassName += ' -is-selected';
          }
          navObj.push(
            <div key={'id' + k + '-' + vnfr.id}  onClick={self.props.loadRecord.bind(self,vnfr)} className={iClassName}>
              <img src={nsdImg} />
              <section id={vnfr.id}>
              <h1 title={vnfr.name}>{vnfr.name}</h1>
                <h2>{vnfr.type}</h2>
              </section>
            </div>
          )
        });
      } else {
        if(n.id == self.props.activeNavID) {
          itemClassName += ' -is-selected';
        }
        navObj.push(
          <div key={'id' + k + '-' + n.id}  onClick={self.props.loadRecord.bind(self,n)} className={itemClassName}>
            <img src={nsdImg} />
            <section id={n.id}>
            <h1 title={n.name}>{n.name}</h1>
              <h2>{n.type}</h2>
            </section>
          </div>
        );
      }
      nav.push(
        <li className={scalingGroupClass} key={"scalingGroupTile-" + k}>
          <div className={scalingGroupTitleClass}>
            {scalingGroupTitle}
          </div>
          {navObj}
        </li>
      )
    })
    if(this.props.isLoading) {
        html = <DashboardCard className="loading" showHeader={true} title="Loading..."><LoadingIndicator size={10} show={true} /></DashboardCard>
    } else {
        html = (
          <DashboardCard showHeader={true} title="Select Record" className={"recordNavigator" + className}>
            <ul className="catalogItems">
              {
                nav
              }
            </ul>
          </DashboardCard>
        );
    }
    return html;
  }
}
RecordNavigator.defaultProps = {
  nav: []
}


