
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React, {Component} from 'react';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import TreeView from 'react-treeview';
import '../../node_modules/react-treeview/react-treeview.css';

function constructTree(data) {
  var markup = [];
  for (var k in data) {
    if (typeof(data[k]) == "object") {
        var html = (
          <TreeView key={k} nodeLabel={k}>
            {constructTree(data[k])}
          </TreeView>
        );
        markup.push(html);
    } else {
     markup.push((<div key={k} className="info">{k} = {data[k].toString()}</div>));
    }
  }
  return markup;
}

function constructVDU(VDU) {
  let markup;
  let props = ["vm-flavor", "guest-epa", "vswitch-epa", "hypervisor-epa", "host-epa"];
  let paramsHTML = [];
  props.map((p, index) => {
    let toPush;
    let item = VDU[p];
    if(item) {
      toPush = (
        <TreeView key={index + '|' + p} nodeLabel={p} >
          {constructTree(VDU[p])}
        </TreeView>
      );
    }
    paramsHTML.push(toPush)
  })
  return  paramsHTML;
}
function constructTopLevel(VNF, i) {
  let html = (
          <TreeView key={i} nodeLabel={VNF.name} defaultCollapsed={true}>
             {
                VNF.vdu.map(function(vdu, i) {
                  return constructVDU(vdu, i);
                })
              }
          </TreeView>
        );
  return html;
}

export default class SLAParams extends Component {
  constructor(props) {
    super(props);
  }
  render() {
    let html;
    let tree;
    try {
      tree = (
        this.props.data.map(function(VDU, VNFi) {
          let VNF = VDU["vnfd:vnfd"];
          return (
            <div key={VNF.name + '|' + VNFi}>
              {constructTopLevel(VNF, VNFi)}
            </div>
          )
        })
      );
     } catch (e) {
      console.warn("Trying to display SLA Params, but there are none available. Check race conditions or API failings");
    };
    html = (
      <DashboardCard
        showHeader={true}
        title={'2. Review EPA Params'}
        className={'specifySLAParams'}
      >
        {tree}
      </DashboardCard>
    )
    return html;
  }
}
SLAParams.defaultProps = {
  data: []
}
