/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import LoadingIndicator from 'widgets/loading-indicator/loadingIndicator.jsx';
import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import Listy from 'widgets/listy/listy.js';

import _ from 'underscore';

export default class TopologyDetail extends React.Component {
    constructor(props) {
        super(props);
    }

    detailData(data) {
        if (_.isEmpty(data)) {
            return {};
        } else {
            return {
                name: data['name'],
                full_name: data['full_name'],
                network: data['network'],
                attr: data['attr']
            }
        }
    }

    rawDetails(data) {
        let text = JSON.stringify(data, undefined, 2);
        if (text == "{}") {
            return false;
        } else {
            return (
                <div className="topologyDebugRawJSON">
                    <h2>Raw JSON</h2>
                    <pre className="language.js">{text}</pre>
                </div>
            );
        }
    }

    render() {
        return (
            <DashboardCard showHeader={true} title="Record Details" className={'recordDetails'}>
              <div className="nodeAttr">
                <Listy data={this.detailData(this.props.data)}
                       noDataMessage={"Select a node"}
                       debugMode={this.props.debugMode}
                />
              </div>
              {
                (this.props.debugMode)
                    ? this.rawDetails(this.props.data)
                    : false
              }
            </DashboardCard>
        );
    }
}

TopologyDetail.propTypes = {
    debugMode: React.PropTypes.bool
}

TopologyDetail.defaultProps = {
    data: {},
    debugMode: false
}
