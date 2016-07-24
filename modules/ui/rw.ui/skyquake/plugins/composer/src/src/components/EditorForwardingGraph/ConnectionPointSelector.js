/**
 * Created by onvelocity on 3/4/16.
 */
'use strict';
import React from 'react'
import ClassNames from 'classnames'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import DescriptorModelFactory from '../../libraries/model/DescriptorModelFactory'
import onHoverHighlightConnectionPoint from './onHoverHighlightConnectionPoint'
import '../../styles/ConnectionPointSelector.scss'
export default function ConnectionPointSelector(props) {

	const containers = props.containers || [];
	const serviceChain = props.serviceChain || 'SF';
	const connectionPointStyle = props.style || {};
	const isDisabled = props.isDisabled;
	const disabledMessage = props.disabledMessage || '';

	// events
	const onExitEditPathMode = props.onExitEditPathMode || (() => {});
	const onAddConnectionPointRef = props.onAddConnectionPointRef || (() => {});

	function mapConnectionPoint(connector) {
		const cpNumber = connector.cpNumber;
		return (
			<div key={connector.uid} className={ClassNames(connector.className, 'connection-point')} style={connectionPointStyle}
				 onClick={onAddConnectionPointRef.bind(null, connector)}
				 onMouseEnter={onHoverHighlightConnectionPoint.bind(null, cpNumber)}
				 onMouseLeave={onHoverHighlightConnectionPoint.bind(null, cpNumber)}><small>cp{cpNumber}</small></div>
		);
	}

	function mapConnectionPointsForConstituentVnfd(cvnfd, i) {
		return (
			<div key={i} className={ClassNames(cvnfd.className, 'vnfd')}>
				<small className="vnfd-title">{cvnfd.title}</small>
				<div className="connectors">
					{cvnfd.connectors.map(mapConnectionPoint)}
				</div>
			</div>
		);
	}

	if (isDisabled) {
		return (<div>{disabledMessage}</div>)
	}

	return (
		<div className="ConnectionPointSelector selection">
			<div className="vnfd-list">
				{containers.filter(d => DescriptorModelFactory.isConstituentVnfdWithServiceChain(d, serviceChain)).map(mapConnectionPointsForConstituentVnfd)}
				<small className="Button"
					   onClick={onExitEditPathMode}>done
				</small>
			</div>
		</div>
	)

}