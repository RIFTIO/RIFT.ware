/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 2/4/16.
 */

'use strict';

import d3 from 'd3'
import React from 'react'
import Range from '../Range'
import Button from '../Button'
import ClassNames from 'classnames'
import changeCase from 'change-case'
import LayoutRow from '../LayoutRow'
import SelectionManager from '../../libraries/SelectionManager'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import CanvasEditorActions from '../../actions/CanvasEditorActions'
import DescriptorModelFactory from '../../libraries/model/DescriptorModelFactory'
import ComposerAppActions from '../../actions/ComposerAppActions'
import DescriptorModelMetaFactory from '../../libraries/model/DescriptorModelMetaFactory'
import ComposerAppStore from '../../stores/ComposerAppStore'
import DeletionManager from '../../libraries/DeletionManager'
import ContentEditableDiv from '../ContentEditableDiv'
import TooltipManager from '../../libraries/TooltipManager'
import HighlightRecordServicePaths from '../../libraries/graph/HighlightRecordServicePaths'
import mapClassifier from './mapClassifier'
import mapRecordServicePath from './mapRecordServicePath'
import onCutDelegateToRemove from './onCutDelegateToRemove'
import onClickSelectAndShowInDetailsPanel from './onClickSelectAndShowInDetailsPanel'

import '../../styles/EditForwardingGraphPaths.scss'

import imgNSD from '../../images/default-catalog-icon.svg'
import imgFG from '../../../../node_modules/open-iconic/svg/infinity.svg'
import imgRemove from '../../../../node_modules/open-iconic/svg/trash.svg'
import imgAdd from '../../../../node_modules/open-iconic/svg/plus.svg'
import imgConnection from '../../../../node_modules/open-iconic/svg/random.svg'
import imgClassifier from '../../../../node_modules/open-iconic/svg/spreadsheet.svg'
import imgReorder from '../../../../node_modules/open-iconic/svg/menu.svg'

function mapFG(fg, i) {

	const context = this;
	context.vnffg = fg;

	const colors = fg.colors;
	const stylePrimary = {borderColor: colors.primary};
	const styleSecondary = {borderColor: colors.secondary};

	context.stylePrimary = stylePrimary;
	context.styleSecondary = styleSecondary;

	const rspMap = fg.rsp.reduce((map, rsp) => {
		map[rsp.id] = rsp;
		rsp.classifier = [];
		return map;
	}, {});

	fg.classifier.forEach(classifier => {
		const rsp = rspMap[classifier.model['rsp-id-ref']];
		if (rsp) {
			rsp.classifier.push(classifier);
		}
	});

	function onClickRemoveForwardingGraph(fg, event) {
		event.preventDefault();
		const root = fg.getRoot();
		fg.remove();
		CatalogItemsActions.catalogItemDescriptorChanged(root);
	}

	function onClickAddClassifier(context, fg, event) {
		event.preventDefault();
		fg.createClassifier();
		CatalogItemsActions.catalogItemDescriptorChanged(fg.getRoot());
	}

	function onClickToggleShowAllFGPaths(fg, event) {
		//event.preventDefault();
		event.stopPropagation();
		fg.uiState.showPaths = event.target.checked;
		fg.rsp.forEach(rsp => rsp.uiState.showPath = event.target.checked);
		CatalogItemsActions.catalogItemMetaDataChanged(fg.getRoot().model);
	}

	if (!fg.uiState.hasOwnProperty('showPaths')) {
		fg.uiState.showPaths = true;
		fg.rsp.forEach(d => d.uiState.showPath = true);
	}

	const toggleSelectAllPaths = (
		<input type="checkbox" name={'show-path' + fg.uid} checked={fg.uiState.showPaths} onChange={() => {}} onClick={onClickToggleShowAllFGPaths.bind(null, fg)} />
	);

	const srpFactory = DescriptorModelFactory.newRecordServicePathFactory({}, fg);
	srpFactory.uid = fg.uid + i;

	const hasServiceFunctionVNFDs = context.containers.filter(d => DescriptorModelFactory.isConstituentVnfdWithServiceChain(d, 'SF')).length > 0;

	return (
		<div key={i} className={fg.className} data-uid={fg.uid} data-offset-width="true" onClick={onClickSelectAndShowInDetailsPanel.bind(null, fg)} onCut={onCutDelegateToRemove.bind(null, fg)}>
			<div key="outline-indicator" data-outline-indicator="true"></div>
			<div className="header-actions">
				<Button className="remove-forwarding-graph" title="Remove" onClick={onClickRemoveForwardingGraph.bind(null, fg)} src={imgRemove}/>
			</div>
			<LayoutRow primaryActionColumn={toggleSelectAllPaths} secondaryActionColumn={<img className="fg-icon" src={imgFG} width="20px" />}>
				<small>{fg.title}</small>
			</LayoutRow>
			<div>
				<h4>Rendered Service Paths</h4>
				{hasServiceFunctionVNFDs ? fg.recordServicePaths.concat(srpFactory).map(mapRecordServicePath.bind(null, context)) : <small className="no-service-function-chain-msg hint">A VNFD with the chain SF is required to build Rendered Service Paths.</small>}
			</div>
			<div>
				<h4>Classifiers</h4>
				{fg.classifier.map(mapClassifier.bind(null, context))}
				<div className="footer-actions">
					<div className="row-action-column">
						<Button className="create-new-classifier" src={imgAdd} width="20px" onClick={onClickAddClassifier.bind(null, context, fg)} label="Add Classifier" />
					</div>
				</div>
			</div>
		</div>
	);

}

function mapNSD(nsd, i) {

	const context = this;
	context.nsd = nsd;

	function onClickAddForwardingGraph(nsd, event) {
		event.preventDefault();
		nsd.createVnffgd();
		CatalogItemsActions.catalogItemDescriptorChanged(nsd.getRoot());
	}

	const forwardingGraphs = nsd.forwardingGraphs.map(mapFG.bind(context));
	if (forwardingGraphs.length === 0) {
		forwardingGraphs.push(
			<div key="1" className="welcome-message">
				No Forwarding Graphs to model.
			</div>
		);
	}

	return (
		<div key={i} className={nsd.className}>
			{forwardingGraphs}
			<div className="footer-actions">
				<div className="row-action-column">
					<Button className="create-new-forwarding-graph" src={imgAdd} width="20px" onClick={onClickAddForwardingGraph.bind(null, nsd)} label="Add new Forwarding Graph" />
				</div>
			</div>
		</div>
	);

}

const EditForwardingGraphPaths = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState: function () {
		return ComposerAppStore.getState();
	},
	getDefaultProps: function () {
		return {
			containers: []
		};
	},
	componentWillMount: function () {
	},
	componentDidMount: function () {
	},
	componentDidUpdate: function () {
	},
	componentWillUnmount: function () {
	},
	render() {

		const containers = this.props.containers;
		const context = {
			component: this,
			containers: containers
		};

		const networkService = containers.filter(d => d.type === 'nsd');
		if (networkService.length === 0) {
			return <p className="welcome-message">No <img src={imgNSD} width="20px" /> NSD open in the canvas. Try opening an NSD.</p>;
		}

		return (
			<div className="EditForwardingGraphPaths -with-transitions" data-offset-parent="true">
				<div key="outline-indicator" data-outline-indicator="true"></div>
				{containers.filter(d => d.type === 'nsd').map(mapNSD.bind(context))}
			</div>
		);

	}
});

export default EditForwardingGraphPaths;
