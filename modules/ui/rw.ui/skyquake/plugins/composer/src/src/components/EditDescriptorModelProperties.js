/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 1/18/16.
 *
 * This class generates the form fields used to edit the CONFD JSON model.
 */
'use strict';

import _ from 'lodash'
import utils from '../libraries/utils'
import React from 'react'
import ClassNames from 'classnames'
import changeCase from 'change-case'
import toggle from '../libraries/ToggleElementHandler'
import Button from './Button'
import Property from '../libraries/model/DescriptorModelMetaProperty'
import ComposerAppActions from '../actions/ComposerAppActions'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import DESCRIPTOR_MODEL_FIELDS from '../libraries/model/DescriptorModelFields'
import DescriptorModelFactory from '../libraries/model/DescriptorModelFactory'
import DescriptorModelMetaFactory from '../libraries/model/DescriptorModelMetaFactory'
import SelectionManager from '../libraries/SelectionManager'
import DeletionManager from '../libraries/DeletionManager'
import DescriptorModelIconFactory from '../libraries/model/IconFactory'
import getEventPath from '../libraries/getEventPath'

import imgAdd from '../../../node_modules/open-iconic/svg/plus.svg'
import imgRemove from '../../../node_modules/open-iconic/svg/trash.svg'

import '../styles/EditDescriptorModelProperties.scss'

function getDescriptorMetaBasicForType(type) {
	const basicPropertiesFilter = d => _.contains(DESCRIPTOR_MODEL_FIELDS[type], d.name);
	return DescriptorModelMetaFactory.getModelMetaForType(type, basicPropertiesFilter) || {properties: []};
}

function getDescriptorMetaAdvancedForType(type) {
	const advPropertiesFilter = d => !_.contains(DESCRIPTOR_MODEL_FIELDS[type], d.name);
	return DescriptorModelMetaFactory.getModelMetaForType(type, advPropertiesFilter) || {properties: []};
}

function getTitle(model = {}) {
	if (typeof model['short-name'] === 'string' && model['short-name']) {
		return model['short-name'];
	}
	if (typeof model.name === 'string' && model.name) {
		return model.name;
	}
	if (model.uiState && typeof model.uiState.displayName === 'string' && model.uiState.displayName) {
		return model.uiState.displayName
	}
	if (typeof model.id === 'string') {
		return model.id;
	}
}

export default function EditDescriptorModelProperties(props) {

	const container = props.container;

	if (!(DescriptorModelFactory.isContainer(container))) {
		return
	}

	function startEditing() {
		DeletionManager.removeEventListeners();
	}

	function endEditing() {
		DeletionManager.addEventListeners();
	}

	function onClickSelectItem(property, path, value, event) {
		event.preventDefault();
		const root = this.getRoot();
		if (SelectionManager.select(value)) {
			CatalogItemsActions.catalogItemMetaDataChanged(root.model);
		}
	}

	function onFocusPropertyFormInputElement(property, path, value, event) {

		event.preventDefault();
		startEditing();

		function removeIsFocusedClass(event) {
			event.target.removeEventListener('blur', removeIsFocusedClass);
			Array.from(document.querySelectorAll('.-is-focused')).forEach(d => d.classList.remove('-is-focused'));
		}

		removeIsFocusedClass(event);

		const propertyWrapper = getEventPath(event).reduce((parent, element) => {
			if (parent) {
				return parent;
			}
			if (!element.classList) {
				return false;
			}
			if (element.classList.contains('property')) {
				return element;
			}
		}, false);

		if (propertyWrapper) {
			propertyWrapper.classList.add('-is-focused');
			event.target.addEventListener('blur', removeIsFocusedClass);
		}

	}

	function buildAddPropertyAction(container, property, path) {
		function onClickAddProperty(property, path, event) {
			event.preventDefault();
			//SelectionManager.resume();
			const create = Property.getContainerCreateMethod(property, this);
			if (create) {
				const model = null;
				create(model, path, property);
			} else {
				const name = path.join('.');
				const value = Property.createModelInstance(property);
				utils.assignPathValue(this.model, name, value);
			}
			CatalogItemsActions.catalogItemDescriptorChanged(this.getRoot());
		}
		return (
				<Button className="inline-hint" onClick={onClickAddProperty.bind(container, property, path)} label="Add" src={imgAdd} />
		);
	}

	function buildRemovePropertyAction(container, property, path) {
		function onClickRemoveProperty(property, path, event) {
			event.preventDefault();
			const name = path.join('.');
			const removeMethod = Property.getContainerMethod(property, this, 'remove');
			if (removeMethod) {
				removeMethod(utils.resolvePath(this.model, name));
			} else {
				utils.removePathValue(this.model, name);
			}
			CatalogItemsActions.catalogItemDescriptorChanged(this.getRoot());
		}
		return (
			<Button className="remove-property-action inline-hint" title="Remove" onClick={onClickRemoveProperty.bind(container, property, path)} label="Remove" src={imgRemove}/>
		);
	}

	function onFormFieldValueChanged(event) {
		if (DescriptorModelFactory.isContainer(this)) {
			event.preventDefault();
			const name = event.target.name;
			const value = event.target.value;
			utils.assignPathValue(this.model, name, value);
			CatalogItemsActions.catalogItemDescriptorChanged(this.getRoot());
		}
	}

	function buildField(container, property, path, value, fieldKey) {

		const name = path.join('.');
		const isEditable = true;
		const isGuid = Property.isGuid(property);
		const onChange = onFormFieldValueChanged.bind(container);
		const isEnumeration = Property.isEnumeration(property);
		const onFocus = onFocusPropertyFormInputElement.bind(container, property, path, value);
		const placeholder = changeCase.title(property.name);
		const className = ClassNames(property.name + '-input', {'-is-guid': isGuid});
		if (isEnumeration) {
			const enumeration = Property.getEnumeration(property, value);
			const options = enumeration.map((d, i) => {
				// note yangforge generates values for enums but the system does not use them
				// so we categorically ignore them
				// https://trello.com/c/uzEwVx6W/230-bug-enum-should-not-use-index-only-name
				//return <option key={fieldKey + ':' + i} value={d.value}>{d.name}</option>;
				return <option key={fieldKey.toString() + ':' + i} value={d.name}>{d.name}</option>;
			});
			const isValueSet = enumeration.filter(d => d.isSelected).length > 0;
			if (!isValueSet || property.cardinality === '0..1') {
				const noValueDisplayText = changeCase.title(property.name);
				options.unshift(<option key={'(value-not-in-enum)' + fieldKey.toString()} value="" placeholder={placeholder}>{noValueDisplayText}</option>);
			}
			return <select key={fieldKey.toString()} id={fieldKey.toString()} className={ClassNames({'-value-not-set': !isValueSet})} name={name} value={value} title={name} onChange={onChange} onFocus={onFocus} onBlur={endEditing} onMouseDown={startEditing} onMouseOver={startEditing} readOnly={!isEditable}>{options}</select>;
		}

		if (property['preserve-line-breaks']) {
			return <textarea key={fieldKey.toString()} cols="5" id={fieldKey.toString()} name={name} value={value} placeholder={placeholder} onChange={onChange} onFocus={onFocus} onBlur={endEditing} onMouseDown={startEditing} onMouseOver={startEditing} onMouseOut={endEditing} onMouseLeave={endEditing} readOnly={!isEditable} />;
		}

		return <input key={fieldKey.toString()}
					  id={fieldKey.toString()}
					  type="text"
					  name={name}
					  value={value}
					  className={className}
					  placeholder={placeholder}
					  onChange={onChange}
					  onFocus={onFocus}
					  onBlur={endEditing}
					  onMouseDown={startEditing}
					  onMouseOver={startEditing}
					  onMouseOut={endEditing}
					  onMouseLeave={endEditing}
					  readOnly={!isEditable}
		/>;

	}

	function buildElement(container, property, valuePath, value) {
		return property.properties.map((property, index) => {
			let childValue;
			const childPath = valuePath.slice();
			if (typeof value === 'object') {
				childValue = value[property.name];
			}
			childPath.push(property.name);

			return build(container, property, childPath, childValue);

		});
	}

	function buildChoice(container, property, path, value, key) {

		function onFormFieldValueChanged(event) {
			if (DescriptorModelFactory.isContainer(this)) {

				event.preventDefault();

				const name = event.target.name;
				const value = event.target.value;

				/*
					Transient State is stored for convenience in the uiState field.
					The choice yang type uses case elements to describe the "options".
					A choice can only ever have one option selected which allows
					the system to determine which type is selected by the name of
					the element contained within the field.
				 */

				//const stateExample = {
				//	uiState: {
				//		choice: {
				//			'conf-config': {
				//				selected: 'rest',
				//				'case': {
				//					rest: {},
				//					netconf: {},
				//					script: {}
				//				}
				//			}
				//		}
				//	}
				//};

				const statePath = ['uiState.choice'].concat(name);
				const stateObject = utils.resolvePath(this.model, statePath.join('.')) || {};
				// write state back to the model so the new state objects are captured
				utils.assignPathValue(this.model, statePath.join('.'), stateObject);

				// write the current choice value into the state
				const choiceObject = utils.resolvePath(this.model, [name, stateObject.selected].join('.'));
				if (choiceObject) {
					utils.assignPathValue(stateObject, ['case', stateObject.selected].join('.'), _.cloneDeep(choiceObject));
				}

				// remove the current choice value from the model
				utils.removePathValue(this.model, [name, stateObject.selected].join('.'));

				// get any state for the new selected choice
				const newChoiceObject = utils.resolvePath(stateObject, ['case', value].join('.')) || {};

				// assign new choice value to the model
				utils.assignPathValue(this.model, [name, value].join('.'), newChoiceObject);

				// update the selected name
				utils.assignPathValue(this.model, statePath.concat('selected').join('.'), value);

				CatalogItemsActions.catalogItemDescriptorChanged(this.getRoot());
			}
		}

		const caseByNameMap = {};

		const onChange = onFormFieldValueChanged.bind(container);

		const cases = property.properties.map(d => {
			if (d.type === 'case') {
				caseByNameMap[d.name] = d.properties[0];
				return {optionName: d.name, optionTitle: d.description};
			}
			caseByNameMap[d.name] = d;
			return {optionName: d.name};
		});

		const options = [{optionName: ''}].concat(cases).map((d, i) => {
			return (
				<option key={i} value={d.optionName} title={d.optionTitle}>
					{d.optionName}
					{i ? null : changeCase.title(property.name)}
				</option>
			);
		});

		const selectName = path.join('.');
		const selectedOptionPath = ['uiState.choice', selectName, 'selected'].join('.');
		const selectedOptionValue = utils.resolvePath(container.model, selectedOptionPath);
		const valueProperty = caseByNameMap[selectedOptionValue] || {properties: []};

		const valueResponse = valueProperty.properties.map((d, i) => {
			const childPath = path.concat(valueProperty.name, d.name);
			const childValue = utils.resolvePath(container.model, childPath.join('.'));
			return (
				<div key={childPath.concat('info', i).join(':')}>
					{build(container, d, childPath, childValue, props)}
				</div>
			);
		});

		const onFocus = onFocusPropertyFormInputElement.bind(container, property, path, value);

		return (
			<div key={key} className="choice">
				<select key={Date.now()} className={ClassNames({'-value-not-set': !selectedOptionValue})} name={selectName} value={selectedOptionValue} onChange={onChange} onFocus={onFocus} onBlur={endEditing} onMouseDown={startEditing} onMouseOver={startEditing} onMouseOut={endEditing} onMouseLeave={endEditing}>
					{options}
				</select>
				{valueResponse}
			</div>
		);

	}

	function buildSimpleListItem(container, property, path, value, key, index) {
		// todo need to abstract this better
		const title = getTitle(value);
		var req = require.context("../", true, /\.svg/);
		return (
			<div>
				<a href="#select-list-item" key={Date.now()} className={property.name + '-list-item simple-list-item '} onClick={onClickSelectItem.bind(container, property, path, value)}>
					<img src={req('./' + DescriptorModelIconFactory.getUrlForType(property.name))} width="20px" />
					<span>{title}</span>
				</a>
				{buildRemovePropertyAction(container, property, path)}
			</div>
		);
	}

	function buildRemoveListItem(container, property, valuePath, fieldKey, index) {
		const className = ClassNames(property.name + '-remove actions');
		return (
			<div key={fieldKey.concat(index).join(':')} className={className}>
				<h3>
					<span className={property.type + '-name name'}>{changeCase.title(property.name)}</span>
					<span className="info">{index + 1}</span>
					{buildRemovePropertyAction(container, property, valuePath)}
				</h3>
			</div>
		);
	}

	function buildLeafListItem(container, property, valuePath, value, key, index) {
		// look at the type to determine how to parse the value
		return (
			<div>
				{buildRemoveListItem(container, property, valuePath, key, index)}
				{buildField(container, property, valuePath, value, key)}
			</div>

		);
	}

	function build(container, property, path, value, props = {}) {

		const fields = [];
		const isLeaf = Property.isLeaf(property);
		const isArray = Property.isArray(property);
		const isObject = Property.isObject(property);
		const isLeafList = Property.isLeafList(property);
		const fieldKey = [container.id].concat(path);
		const isRequired = Property.isRequired(property);
		const title = changeCase.titleCase(property.name);
		const columnCount = property.properties.length || 1;
		const isColumnar = isArray && (Math.round(props.width / columnCount) > 155);
		const classNames = {'-is-required': isRequired, '-is-columnar': isColumnar};

		if (!property.properties && isObject) {
			const uiState = DescriptorModelMetaFactory.getModelMetaForType(property.name) || {};
			property.properties = uiState.properties;
		}

		const hasProperties = _.isArray(property.properties) && property.properties.length;
		const isMissingDescriptorMeta = !hasProperties && !Property.isLeaf(property);

		// ensure value is not undefined for non-leaf property types
		if (isObject) {
			if (typeof value !== 'object') {
				value = isArray ? [] : {};
			}
		}
		const valueAsArray = _.isArray(value) ? value : isLeafList && typeof value === 'undefined' ? [] : [value];

		const isMetaField = property.name === 'meta';
		const isCVNFD = property.name === 'constituent-vnfd';
		const isSimpleListView = Property.isSimpleList(property);

		valueAsArray.forEach((value, index) => {

			let field;
			const key = fieldKey.slice();
			const valuePath = path.slice();

			if (isArray) {
				valuePath.push(index);
				key.push(index);
			}

			if (isMetaField) {
				if (typeof value === 'object') {
					value = JSON.stringify(value, undefined, 12);
				} else if (typeof value !== 'string') {
					value = '{}';
				}
			}

			if (isMissingDescriptorMeta) {
				field = <span key={key.concat('warning').join(':')} className="warning">No Descriptor Meta for {property.name}</span>;
			} else if (property.type === 'choice') {
				field = buildChoice(container, property, valuePath, value, key.join(':'));
			} else if (isSimpleListView) {
				field = buildSimpleListItem(container, property, valuePath, value, key, index);
			} else if (isLeafList) {
				field = buildLeafListItem(container, property, valuePath, value, key, index);
			} else if (hasProperties) {
				field = buildElement(container, property, valuePath, value, key.join(':'))
			} else {
				field = buildField(container, property, valuePath, value, key.join(':'));
			}

			function onClickLeaf(property, path, value, event) {
				if (event.isDefaultPrevented()) {
					return;
				}
				event.preventDefault();
				event.stopPropagation();
				this.getRoot().uiState.focusedPropertyPath = path.join('.');
				console.log('property selected', path.join('.'));
				ComposerAppActions.propertySelected([path.join('.')]);
			}

			const clickHandler = isLeaf ? onClickLeaf : () => {};
			const isContainerList = isArray && !(isSimpleListView || isLeafList);

			fields.push(
				<div key={fieldKey.concat(['property-content', index]).join(':')}
					 className={ClassNames('property-content', {'simple-list': isSimpleListView})}
					 onClick={clickHandler.bind(container, property, valuePath, value)}>
					{isContainerList ? buildRemoveListItem(container, property, valuePath, fieldKey, index) : null}
					{field}
				</div>
			);

		});

		classNames['-is-leaf'] = isLeaf;
		classNames['-is-array'] = isArray;
		classNames['cols-' + columnCount] = isColumnar;

		if (property.type === 'choice') {
			value = utils.resolvePath(container.model, ['uiState.choice'].concat(path, 'selected').join('.'));
		}

		let displayValue = typeof value === 'object' ? '' : value;
		const displayValueInfo = isArray ? valueAsArray.filter(d => typeof d !== 'undefined').length + ' items' : '';

		const onFocus = isLeaf ? event => event.target.classList.add('-is-focused') : false;

		return (
			<div key={fieldKey.join(':')} className={ClassNames(property.type + '-property property', classNames)} onFocus={onFocus}>
				<h3 className="property-label">
					<label htmlFor={fieldKey.join(':')}>
						<span className={property.type + '-name name'}>{title}</span>
						<small>
							<span className={property.type + '-info info'}>{displayValueInfo}</span>
							<span className={property.type + '-value value'}>{displayValue}</span>
						</small>
						{isArray ? buildAddPropertyAction(container, property, path.concat(valueAsArray.length)) : null}
					</label>
				</h3>
				<span className={property.type + '-description description'}>{property.description}</span>
				<val className="property-value">
					{isCVNFD ? <span className={property.type + '-tip tip'}>Drag a VNFD from the Catalog to add more.</span> : null}
					{fields}
				</val>
			</div>
		);

	}

	const containerType = container.uiState['qualified-type'] || container.uiState.type;
	const basicProperties = getDescriptorMetaBasicForType(containerType).properties;

	function buildBasicGroup() {
		if (basicProperties.length === 0) {
			return null;
		}
		return (
			<div className="basic-properties-group">
				<h2>Basic</h2>
				<div>
					{basicProperties.map(property => {
						const path = [property.name];
						const value = container.model[property.name];
						return build(container, property, path, value);
					})}
				</div>
			</div>
		);
	}

	function buildAdvancedGroup() {
		const properties = getDescriptorMetaAdvancedForType(containerType).properties;
		if (properties.length === 0) {
			return null;
		}
		const hasBasicFields = basicProperties.length > 0;
		const closeGroup = basicProperties.length > 0;
		return (
			<div className="advanced-properties-group">
				<h1 data-toggle={closeGroup ? 'true' : 'false'} className={ClassNames({'-is-toggled': closeGroup})} onClick={toggle} style={{display: hasBasicFields ? 'block' : 'none'}}>
					<a className="toggle-show-more" href="#show-more-properties">more&hellip;</a>
					<a className="toggle-show-less" href="#show-more-properties">less&hellip;</a>
				</h1>
				<div className="toggleable">
					{properties.map(property => {
						const path = [property.name];
						const value = container.model[property.name];
						return build(container, property, path, value, {toggle: true, width: props.width});
					})}
				</div>
				<div className="toggle-bottom-spacer" style={{visibility: 'hidden', 'height': '50%', position: 'absolute'}}>We need this so when the user closes the panel it won't shift away and scare the bj out of them!</div>
			</div>
		);
	}

	function buildMoreLess(d, i) {
		return (
			<span key={'bread-crumb-part-' + i}>
				<a href="#select-item" onClick={onClickSelectItem.bind(d, null, null, d)}>{d.title}</a>
				<i> / </i>
			</span>
		);
	}

	const path = [];
	if (container.parent) {
		path.push(container.parent);
	}
	path.push(container);

	return (
		<div className="EditDescriptorModelProperties -is-tree-view">
			<h1>{path.map(buildMoreLess)}</h1>
			{buildBasicGroup()}
			{buildAdvancedGroup()}
		</div>
	);

}
