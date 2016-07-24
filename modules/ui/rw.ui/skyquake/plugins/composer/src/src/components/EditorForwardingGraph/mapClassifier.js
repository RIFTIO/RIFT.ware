/**
 * Created by onvelocity on 3/2/16.
 */

'use strict';

import d3 from 'd3'
import React from 'react'
import Button from '../Button'
import LayoutRow from '../LayoutRow'
import ContentEditableDiv from '../ContentEditableDiv'
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
import ConnectionPointSelector from './ConnectionPointSelector'
import mapConnectionPoint from './mapConnectionPoint'
import EditableProperty from './EditableProperty'
import onCutDelegateToRemove from './onCutDelegateToRemove'
import onClickSelectAndShowInDetailsPanel from './onClickSelectAndShowInDetailsPanel'
import onFormInputChangedModifyContainerAndNotify from './onFormInputChangedModifyContainerAndNotify'

import imgNSD from '../../images/default-catalog-icon.svg'
import imgFG from '../../../../node_modules/open-iconic/svg/infinity.svg'
import imgRemove from '../../../../node_modules/open-iconic/svg/trash.svg'
import imgAdd from '../../../../node_modules/open-iconic/svg/plus.svg'
import imgConnection from '../../../../node_modules/open-iconic/svg/random.svg'
import imgClassifier from '../../../../node_modules/open-iconic/svg/spreadsheet.svg'
import imgReorder from '../../../../node_modules/open-iconic/svg/menu.svg'

export default function mapClassifier(context, classifier, i) {

	// todo if a classifier is linked to an rsp then highlight it
	//rsp.uiState.showPath = rsp.uiState.hasOwnProperty('showPath') ? rsp.uiState.showPath : true;

	function onInputUpdateModel(context, attr, name, event) {
		event.preventDefault();
		attr.setFieldValue(name, event.target.value);
		CatalogItemsActions.catalogItemDescriptorChanged(attr.getRoot());
	}

	function onClickAddNewMatchAttributes(context, classifier) {
		event.preventDefault();
		event.stopPropagation();
		const newMatchAttr = classifier.createMatchAttributes();
		SelectionManager.disableOutlineChanges();
		CatalogItemsActions.catalogItemDescriptorChanged(classifier.getRoot());
		setTimeout(() => {
			SelectionManager.enableOutlineChanges();
			SelectionManager.select(newMatchAttr);
			SelectionManager.refreshOutline();
			const input = Array.from(document.querySelectorAll(`tr[data-uid="${newMatchAttr.uid}"] input`)).forEach((element, index) => {
				// index 0 is hidden id field
				if (index === 1) {
					element.focus();
				}
			});
		}, 100);
	}

	function mapClassifierMatchAttributes(context, matchAttributes, key) {
		const fields = matchAttributes.fieldNames.map((name, i) => {
			return (
				<td key={i} className={name + '-property'}>
					<div className="match-attr-name">{name}</div>
					<ContentEditableDiv value={matchAttributes.getFieldValue(name)}
										onBlur={() => DeletionManager.addEventListeners()}
										onClick={event => {
											event.preventDefault();
											SelectionManager.select(matchAttributes);
											SelectionManager.refreshOutline();
										}}
										onCut={event => {
											event.stopPropagation();
										}}
										onChange={onInputUpdateModel.bind(null, context, matchAttributes, name)}
										className="match-attr-value"/>
				</td>
			);
		});
		return (
			<tr key={key}
				data-uid={matchAttributes.uid}
				onCut={event => {
					event.stopPropagation();
					matchAttributes.remove();
					CatalogItemsActions.catalogItemDescriptorChanged(matchAttributes.getRoot());
					SelectionManager.refreshOutline();
				}}>
				{fields}
			</tr>
		)
	}

	function buildRecordServicePathSelector(classifier) {
		const rspId = classifier.model['rsp-id-ref'];
		const options = [{}].concat(classifier.parent.recordServicePaths).map((rsp, i) => {
			return (
				<option key={i} name="rsp-id-ref" value={rsp.id}>{rsp.title}</option>
			)
		});
		return (
			<select name="rsp-id-ref" value={rspId} onChange={onFormInputChangedModifyContainerAndNotify.bind(null, classifier)}>{options}</select>
		)
	}

	function onClickExitPathEdithMode(component, event) {
		event.preventDefault();
		component.setState({editClassifierConnectionPointRef: false});
	}

	function onClickAddConnectionPointRef(component, classifier, connector, event) {
		event.preventDefault();
		classifier.addVnfdConnectionPoint(connector);
		CatalogItemsActions.catalogItemDescriptorChanged(classifier.getRoot());
	}

	function onClickOpenConnectionPointSelector(component, classifier, event) {
		component.setState({editClassifierConnectionPointRef: classifier.uid});
		function closeAndRemoveHandler() {
			component.setState({editClassifierConnectionPointRef: false});
			document.body.removeEventListener('click', closeAndRemoveHandler, true);
		}
		document.body.addEventListener('click', closeAndRemoveHandler, true);
	}

	const attributeNames = DescriptorModelMetaFactory.getModelFieldNamesForType('nsd.vnffgd.classifier.match-attributes');

	const selectedConnectionPoint = classifier.uiState.vnfdRef ? `${classifier.uiState.vnfdRef.name}/${classifier.vnfdConnectionPointRef.vnfdIndex}/${classifier.vnfdConnectionPointRef.vnfdConnectionPointName}` : '';

	const isEditClassifierConnectionPointRef = context.component.state.editClassifierConnectionPointRef === classifier.uid;

	const hasClassifierServiceFunctionVNFDs = context.containers.filter(d => DescriptorModelFactory.isConstituentVnfdWithServiceChain(d, 'CLASSIFIER')).length > 0;

	return (
		<LayoutRow key={classifier.uid}
				   data-uid={classifier.uid}
				   data-offset-width="true"
				   className={ClassNames('fg-classifier', classifier.className, {'-is-edit-classifier-connection-point-ref': isEditClassifierConnectionPointRef})}
				   onClick={onClickSelectAndShowInDetailsPanel.bind(null, classifier)}>
			<div className="classifier-properties">
				<div className="classifier-property">
					<EditableProperty title="name">
						<ContentEditableDiv name="name"
											value={classifier.name}
											autoPadRight="true"
											onBlur={() => DeletionManager.addEventListeners()}
											onChange={onFormInputChangedModifyContainerAndNotify.bind(null, classifier)}
											className="classifier-name" />
					</EditableProperty>
				</div>
				<div className="classifier-property">
					<EditableProperty title="path">
						{buildRecordServicePathSelector(classifier)}
					</EditableProperty>
				</div>
				<div className="classifier-property">
					<EditableProperty title="connection point ref" disabled={!hasClassifierServiceFunctionVNFDs}>
						<ContentEditableDiv autoPadRight="true"
											value={selectedConnectionPoint}
											disabled={!hasClassifierServiceFunctionVNFDs}
											onChange={() => {}}
											onClick={onClickOpenConnectionPointSelector.bind(null, context.component, classifier)} />
					</EditableProperty>
					<div className="select-connection-point-ref">
						<ConnectionPointSelector containers={context.containers}
												 style={context.styleSecondary}
												 serviceChain="CLASSIFIER"
												 isDisabled={!hasClassifierServiceFunctionVNFDs}
												 onExitEditPathMode={onClickExitPathEdithMode.bind(null, context.component)}
												 onAddConnectionPointRef={onClickAddConnectionPointRef.bind(null, context.component, classifier)}
						/>
					</div>
					{!hasClassifierServiceFunctionVNFDs ? <div className="hint">A VNFD with the chain CLASSIFIER is required to add a connection point ref.</div> : ''}
				</div>
			</div>
			<table className="classifier-match-attributes">
				<thead>
					<tr>
						{attributeNames.map((name, i) => <th key={i} className={ClassNames(name + '-property')}>{changeCase.title(name)}</th>)}
					</tr>
				</thead>
				<tbody>
					{classifier.matchAttributes.map(mapClassifierMatchAttributes.bind(null, context))}
				</tbody>
				<tfoot>
					<tr className="xfooter-actions">
						<th colSpan={attributeNames.length} className="row-action-column">
							<Button className="create-new-match-attributes" src={imgAdd} width="20px" onClick={onClickAddNewMatchAttributes.bind(null, context, classifier)} label="Add Match Attributes" />
						</th>
					</tr>
				</tfoot>
			</table>
		</LayoutRow>
	);

}
