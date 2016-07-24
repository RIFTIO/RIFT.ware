import React from 'react';

import DashboardCard from 'widgets/dashboard_card/dashboard_card.jsx';
import LaunchpadCard from '../launchpad_card/launchpadCard.jsx';
import LaunchpadFleetActions from'../launchpadFleetActions';

export default class NsCardPanel extends React.Component {

	onCloseCard(nsr_id) {
		return () => {
			LaunchpadFleetActions.closeNsrCard(nsr_id);
		}
	}
  findNsr(nsrs, nsr_id) {
    nsrs.find()
  }
  render() {
    const {nsrs, openedNsrIDs, ...props} = this.props;
		return (
      <DashboardCard className="nsCardPanel"
          showHeader={true} title="NETWORK SERVICE DETAILS">
                {
                  openedNsrIDs.map((nsr_id, index) => {
                      let nsr = nsrs.find(e => { return e.id == nsr_id; })
                      if (nsr) {
                        return  (
                          <LaunchpadCard deleting={nsr.deleting}
                                slideno={this.props.slideno}
                                key={index}
                                id={nsr_id}
                                name={nsr.name}
                                data={nsr.data}
                                nsr={nsr}
                                isActive={nsr["admin-status"] == "ENABLED"}
                                closeButtonAction={this.onCloseCard(nsr.id)}/>
                        );
                      }
                    }
                  )
                }
        </DashboardCard>
      );
	}
}
NsCardPanel.defaultProps = {
	nsrs: [],
	slideno:0
}
