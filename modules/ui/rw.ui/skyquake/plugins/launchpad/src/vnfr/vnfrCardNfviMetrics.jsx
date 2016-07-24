
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import Bullet from 'widgets/bullet/bullet.js';
class VnfrCardNfviMetrics extends React.Component {
  constructor(props) {
    super(props);
  }
  render() {
    let height = 100;
    let html = (
       <div className="nfviMetrics">
          <div>
            <h3>{this.props.metrics[0].label}</h3>
            <h3>{this.props.metrics[0].total}</h3>
          </div>
          <div>
            <h3>{this.props.metrics[1].label}</h3>
            <Bullet textMarginY={height - (height/2) + 15} fontSize={height / 2} height={height} value={Math.round(this.props.metrics[1].utilization * 100)} />
          </div>
          <div>
            <h3>{this.props.metrics[2].label}</h3>
            <Bullet textMarginY={height - (height/2) + 15} fontSize={height / 2}  height={height} value={Math.round(this.props.metrics[2].utilization * 100)} />
          </div>
        </div>
    );
    return html;
  }
}

export default VnfrCardNfviMetrics;
