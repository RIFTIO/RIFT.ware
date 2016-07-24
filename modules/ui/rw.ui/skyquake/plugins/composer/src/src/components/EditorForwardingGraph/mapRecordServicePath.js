/**
 * Created by onvelocity on 3/2/16.
 */

'use strict';

import d3 from 'd3'
import React from 'react'
import Button from '../Button'
import LayoutRow from '../LayoutRow'
import ContentEditableDiv from '../ContentEditableDiv'
import EditableProperty from './EditableProperty'
import changeCase from 'change-case'
import ClassNames from 'classnames'
import DescriptorModelFactory from '../../libraries/model/DescriptorModelFactory'
import DescriptorModelMetaFactory from '../../libraries/model/DescriptorModelMetaFactory'
import HighlightRecordServicePaths from '../../libraries/graph/HighlightRecordServicePaths'
import ComposerAppActions from '../../actions/ComposerAppActions'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import SelectionManager from '../../libraries/SelectionManager'
import DeletionManager from '../../libraries/DeletionManager'
import TooltipManager from '../../libraries/TooltipManager'
import mapConnectionPoint from './mapConnectionPoint'
import ConnectionPointSelector from './ConnectionPointSelector'
import onCutDelegateToRemove from './onCutDelegateToRemove'
import onClickSelectAndShowInDetailsPanel from './onClickSelectAndShowInDetailsPanel'
import onFormInputChangedModifyContainerAndNotify from './onFormInputChangedModifyContainerAndNotify'
import onHoverHighlightConnectionPoint from './onHoverHighlightConnectionPoint'

import imgNSD from '../../images/default-catalog-icon.svg'
import imgFG from '../../../../node_modules/open-iconic/svg/infinity.svg'
import imgRemove from '../../../../node_modules/open-iconic/svg/trash.svg'
import imgAdd from '../../../../node_modules/open-iconic/svg/plus.svg'
import imgConnection from '../../../../node_modules/open-iconic/svg/random.svg'
import imgClassifier from '../../../../node_modules/open-iconic/svg/spreadsheet.svg'
import imgReorder from '../../../../node_modules/open-iconic/svg/menu.svg'

export default function mapRecordServicePath (context, rsp, i) {

	rsp.uiState.showPath = rsp.uiState.hasOwnProperty('showPath') ? rsp.uiState.showPath : true;

	function removeHighlighting() {
		HighlightRecordServicePaths.removeHighlighting();
	}

	function highlightPath(rsp) {
		HighlightRecordServicePaths.highlightPaths(rsp);
	}

	function onClickRemoveRecordServicePath(rsp, event) {
		event.preventDefault();
		const root = rsp.getRoot();
		rsp.remove();
		CatalogItemsActions.catalogItemDescriptorChanged(root);
	}

	function onClickEnterPathEdithMode(component, rspUid, event) {
		event.preventDefault();
		component.setState({editPathsMode: rspUid});
	}

	function onClickToggleRSPShowPath(rsp, event) {
		// warn preventing default will undo the user's action
		//event.preventDefault();
		event.stopPropagation();
		rsp.uiState.showPath = event.target.checked;
		rsp.parent.uiState.showPaths = rsp.parent.rsp.filter(rsp => rsp.uiState.showPath === true).length === rsp.parent.rsp.length;
		CatalogItemsActions.catalogItemMetaDataChanged(rsp.getRoot().model);
	}

	function onClickExitPathEdithMode(component, event) {
		event.preventDefault();
		component.setState({editPathsMode: false});
	}

	function onClickAddConnectionPointRef(component, rsp, connector, event) {
		event.preventDefault();
		if (rsp.isFactory) {
			const newRsp = rsp.createVnfdConnectionPointRef(connector);
			component.setState({editPathsMode: newRsp.uid});
		} else {
			rsp.createVnfdConnectionPointRef(connector);
		}
		CatalogItemsActions.catalogItemDescriptorChanged(rsp.getRoot());
	}

	const isEditPathsMode = context.component.state.editPathsMode === rsp.uid;

	const toggleSelectionOrCreateNewPath = (
		<div>
			{!rsp.isFactory ? <input type="checkbox" id={'show-path-' + rsp.uid} checked={rsp.uiState.showPath} onChange={() => {}} onClick={onClickToggleRSPShowPath.bind(null, rsp)} /> : ' '}
		</div>
	);

	const editRspName = (
		<EditableProperty title="name">
			<ContentEditableDiv name="name" value={rsp.name} autoPadRight="true" onChange={onFormInputChangedModifyContainerAndNotify.bind(null, rsp)} />
		</EditableProperty>
	);

	const hasServiceFunctionVNFDs = context.containers.filter(d => DescriptorModelFactory.isConstituentVnfdWithServiceChain(d, 'SF')).length > 0;

	if (!hasServiceFunctionVNFDs && rsp.isFactory) {
		return null;
	}

	let cpRefMapped = rsp.connectionPoints.map((cpRef, i) => {
		return {
			index: i,
			value: cpRef
		}
	});

	cpRefMapped.sort((a, b) => {
		return a.value.order - b.value.order;
	});

	let connectionPointsSorted = cpRefMapped.map((cpRef) => {
		return rsp.connectionPoints[cpRef.index];
	});


	return (
		<div key={i} data-offset-width="table.fg-classifier" data-uid={rsp.uid}
			 onClick={onClickSelectAndShowInDetailsPanel.bind(null, rsp)}
			 onMouseOver={highlightPath.bind(null, rsp)}
			 onMouseOut={removeHighlighting.bind(null, rsp)}
			 onMouseLeave={removeHighlighting.bind(null, rsp)}
			 onCut={onCutDelegateToRemove.bind(null, rsp)}>
			<div className={ClassNames(rsp.className, {'-is-factory': rsp.isFactory, '-is-edit-paths-mode': isEditPathsMode})}>
				<LayoutRow primaryActionColumn={toggleSelectionOrCreateNewPath} secondaryActionColumn={null}>
					{editRspName}
					<div className="connection-points">
						{connectionPointsSorted.map(mapConnectionPoint.bind(null, context.stylePrimary, true))}
						{hasServiceFunctionVNFDs ? <div className="rsp-create-new-connection-point-line rsp-line" style={context.styleSecondary}></div> : null}
						{hasServiceFunctionVNFDs ? <div className="enter-path-edit-mode connection-point" style={context.styleSecondary}
							 onClick={onClickEnterPathEdithMode.bind(null, context.component, rsp.uid)}>
							<small>+CP</small>
						</div> : null}
						{hasServiceFunctionVNFDs ? <ConnectionPointSelector containers={context.containers}
												 style={context.styleSecondary}
												 serviceChain="SF"
												 onExitEditPathMode={onClickExitPathEdithMode.bind(null, context.component)}
												 onAddConnectionPointRef={onClickAddConnectionPointRef.bind(null, context.component, rsp)}
						/> : null}
						{!hasServiceFunctionVNFDs && !rsp.isFactory ? <small className="hint">A VNFD with the chain SF is required to build Rendered Service Paths.</small> : null}
						{rsp.isFactory && !isEditPathsMode? <small className="enter-path-edit-mode-hint hint">Tap to start creating a new path.</small> : null}
					</div>
				</LayoutRow>
			</div>
		</div>
	);

}
