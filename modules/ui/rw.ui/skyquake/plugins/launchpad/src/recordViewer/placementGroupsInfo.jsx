
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
export default class PlacementGroupsInfo extends React.Component {
  constructor(props) {
    super(props);
    this.state = {};
    this.state.dict = {};
    this.state.displayedGroup = null;
  }
  componentWillReceiveProps(props) {
    this.setState({
      nsr: props.nsr,
      dict: buildPlacementGroupDictFromVDUR(props.nsr),
      // dict: buildPlacementGroupDict(props.nsr),
    });
  }
  showDisplayGroup = (context, ref, type) => {
    // ref = 'Orcus';
    context.setState({
      displayedGroup: this.state.dict[ref]
    })
  }
  render(){
    let html;
    let vdurs = <VduPlacement nsr={this.props.nsr} showDisplayGroup={this.showDisplayGroup.bind(null, this)}/>;
    let details = <PlacementGroupDetails displayedGroup={this.state.displayedGroup} />;
    return <div className="placementGroups">{vdurs}{details}</div>;
  }
}
PlacementGroupsInfo.defaultProps = {
  nsr: {}
}


class VduPlacement extends React.Component {
  render() {
    let html;
    let {showDisplayGroup, nsr, ...props} = this.props;
    html = (
      <div className="panel panel-5">
        <h3 className="sectionHeader">
          VDU Placement
        </h3>
        <div className="vdupTable">
          <div className="vdup-title">
            <h4>Record</h4>
            <h4>Placement Group</h4>
          </div>
          <dl className="nsrType">
            <dt>
              <div>NSR: {nsr.name}</div>
            </dt>
            <dd></dd>
          </dl>
          {
            flattenPlacementGroupsToHTML(nsr, showDisplayGroup)
          }
        </div>
      </div>
    );
    return html;
  }
}
function flattenPlacementGroupsToHTML(nsr, onClick) {
  let data = [];
  nsr.vnfrs && nsr.vnfrs.map(function(v, i) {
    let html;
    html = (
      <dl className="vnfrType" key={'vnfr-' + i}>
        <dt>
          <div>VNFR: {v['short-name']}</div>
        </dt>
        <dd></dd>
      </dl>
    )
    data.push(html);
    v && v.vdur && v.vdur.map(function(vd, j) {
      let html;
      html = (
        <dl className="vduType" key={'vdur-' + i + '-'+ j}>
          <dt>
            <div>VDUR: {vd['name']}</div>
          </dt>
          <dd>
            <ul>
              {
                vd['placement-groups-info'] && vd['placement-groups-info'].map(function(p, k) {
                  return <li onClick={onClick.bind(null, p.name)} key={'pg-' + i + '-' + j + '-' + k}>{p.name}</li>
                })
              }
            </ul>
          </dd>
        </dl>
      );
      data.push(html);
    });
  });
  return data;
}
class PlacementGroupDetails extends React.Component {
  render() {
    let html;
    let {displayedGroup, ...props} = this.props;
    let dg = displayedGroup;
    let dgHTML = '';
    if(dg) {
      dgHTML = (
        <dl className="placementGroupDetails">
            <dt>Name</dt>
            <dd>{dg.name}</dd>
            <dt>Requirement</dt>
            <dd>{dg.requirement}</dd>
            <dt>Strategy</dt>
            <dd>{dg.strategy}</dd>
            <dt>Cloud Provider</dt>
            <dd>{dg['cloud-type']}</dd>
            {
              (dg['availability-zone'] != undefined) ?
                <dt>Availability Zone</dt> : ''
            }
            {
              (dg['availability-zone'] != undefined) ?
                <dd>{dg['availability-zone']}</dd> : ''
            }
            {
              (dg['server-group'] != undefined) ?
                <dt>Affinity/Anti-Affinity Server Group</dt> : ''
            }
            {
              (dg['server-group'] != undefined) ?
                 <dd>{dg['server-group']}</dd> : ''
            }
            {
              (dg['host-aggregate'] != undefined) ?
                <dt>Host Aggregates</dt> : ''
            }
            {
              dg['host-aggregate'] != undefined ?
                <dd>
                  {dg['host-aggregate'] && dg['host-aggregate'].map(function(ha, h) {
                    return <div key={h}>{ha['metadata-key']}:{ha['metadata-value']}</div>
                  })}
                </dd>
              : ''
            }
        </dl>
      )
    }
    html = (
      <div className="panel panel-5">
        <h3 className="sectionHeader">
          Placement Group Details
        </h3>
        <div>
          {dgHTML}
        </div>
      </div>
    );
    return html;
  }
}
PlacementGroupDetails.defaultProps = {
  displayedGroup: {}
}
function buildPlacementGroupDictFromVDUR(data) {
  let d = {};
  data.vnfrs && data.vnfrs.map(function(vnf) {
    vnf.vdur && vnf.vdur.map(function(v) {
      v['placement-groups-info'] && v['placement-groups-info'].map(function(p) {
        d[p.name] = p;
      })
    })
  })
  return d;
}
function buildPlacementGroupDict(data) {
  let d = {};
  let nsd = data['nsd-placement-group-maps'];
  let vnfd = data['vnfd-placement-group-maps'];
  nsd && nsd.map(function(m, i) {
    d[m['placement-group-ref']] = m;
  });
  vnfd && vnfd.map(function(m, i) {
    d[m['placement-group-ref']] = m;
  });
  return d;
}
