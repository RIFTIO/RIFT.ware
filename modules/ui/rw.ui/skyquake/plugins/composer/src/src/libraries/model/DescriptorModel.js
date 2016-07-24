
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/23/15.
 */

import _ from 'lodash'
import guid from '../guid'
import Position from '../graph/Position'
import IconFactory from './IconFactory'
import SelectionManager from '../SelectionManager'
import DescriptorModelMetaFactory from './DescriptorModelMetaFactory'

/**
 * Abstract base class for the CONFD MANO Descriptors as defined by the Rift.ware YANG configuration files.
 *
 * Sub-classes should specify the type and qualified-type properties.
 */
export default class DescriptorModel {

	constructor(model = {uiState: {}}, parent = null) {
		// when our instance has no more strong references
		// then our properties will get garbage collected.
		this._props_ = new WeakMap();
		this._props_.set(this, {
			position: new Position(),
			children: new Set(),
			values: {},
			model: model
		});
		this.className = 'DescriptorModel';
		this.uid = this.uid || guid();
		if (parent instanceof DescriptorModel) {
			parent.addChild(this);
		}
	}

	get fieldNames() {
		return DescriptorModelMetaFactory.getModelFieldNamesForType(this.uiState['qualified-type'] || this.type);
	}

	get property() {
		return DescriptorModelMetaFactory.getModelMetaForType(this.uiState['qualified-type'] || this.type);
	}

	get model() {
		let model = this._props_.get(this).model;
		if (!model) {
			model = this.model = {uiState: {}};
		}
		return model;
	}

	set model(model) {
		this._props_.get(this).model = model;
		return this;
	}

	get uiState() {
		return this.model.uiState = this.model.uiState || {};
	}

	set uiState(uiState) {
		this.model.uiState = uiState;
		return this;
	}

	get uid() {
		return this.uiState[':uid'];
	}

	set uid(uid) {
		this.uiState[':uid'] = uid;
		return this;
	}

	get key() {
		return this.id;
	}

	// key is read only by design

	get id() {
		return this.model.id;
	}

	set id(id) {
		this.model.id = id;
		return this;
	}

	get title() {
		return this.model['short-name'] || this.model.name || this.key;
	}

	// title is read only by design

	get name() {
		return this.model.name;
	}

	set name(name) {
		this.model.name = name;
	}

	get 'short-name'() {
		return this.model['short-name'];
	}

	set 'short-name'(name) {
		this.model['short-name'] = name;
	}

	get type() {
		return this.uiState.type;
	}

	set type(type) {
		this.uiState.type = type;
		return this;
	}

	get qualifiedType() {
		return this.uiState['qualified-type'] || this.type;
	}

	set qualifiedType(type) {
		this.uiState['qualified-type'] = type;
	}

	get position() {
		return this._props_.get(this).position;
	}

	set position(position) {
		if (!(position instanceof Position)) {

		}
		this._props_.get(this).position = position;
		return this;
	}

	get location() {
		return this.uiState.location;
	}

	set location(v) {
		this.uiState.location = v;
	}

	get children() {
		return Array.from(this._props_.get(this).children);
	}

	addProp(name, value) {
		this._props_.get(this).values[name] = value;
		return this;
	}

	getProp(name) {
		return this._props_.get(this).values[name];
	}

	addChild(child) {
		if (!child instanceof DescriptorModel) {
			throw new ReferenceError('child must be an instance of DescriptorModel class');
		}
		if (this.findChildByUid(child.uid)) {
			throw new ReferenceError('child already exists');
		}
		if (child.parent instanceof DescriptorModel) {
			throw new ReferenceError('child already has a parent');
		}
		child.parent = this;
		this._props_.get(this).children.add(child);
	}

	findChildByUid(uid) {
		// uid can be a DescriptorModel instance,  JSON model object, or a uid string
		if (typeof uid === 'object') {
			uid = uid.uiState && uid.uiState[':uid'];
		}
		if (typeof uid === 'string') {
			return this.children.filter(d => d.uid === uid)[0];
		}
	}

	removeChild(child) {
		let uid;
		// warn don't clear parent so that removed ones can get to root for updating the catalog json model
		//child.parent = null;
		// uid can be a DescriptorModel instance, JSON model object, or a uid string
		if (typeof uid === 'object') {
			uid = uid.uiState && uid.uiState[':uid'];
		}
		if (typeof uid === 'string') {
			this.children.filter(d => d.uid === uid).forEach(d => this._props_.get(this).children.delete(d));
		} else {
			this._props_.get(this).children.delete(child);
		}
	}

	getRoot() {
		let root = this;
		while (root && root.parent) {
			root = root.parent;
		}
		return root;
	}

	getRootNSDContainer() {
		let container = this.parent;
		while(container) {
			if (container.type === 'nsd') {
				return container;
			}
			container = container.parent;
		}
	}

	setPosition(position) {
		this.position = new Position(position);
	}

	get selected() {
		return SelectionManager.isSelected(this);
	}

	get colors() {
		return this.uiState.colors || {primary: 'black', secondary: 'white'};
	}

	set colors(colors) {
		this.uiState.colors = colors;
	}

	get icon() {
		return IconFactory.getIconForType(this.type);
	}

	get width() {
		return this.position.width;
	}

	get height() {
		return this.position.height;
	}

	get displayData() {
		return {
			title: this.title,
			type: this.model.type,
			cpNumber: this.cpNumber
		};
	}

	valueOf() {
		return this.model;
	}

	updateModelList(modelFieldName, modelFieldValue, descriptorClass = DescriptorModel, newItemAddedSuccessCallback = () => {}) {
		// value can be Array of (DescriptorModel | json model), DescriptorModel, or json model
		if (_.isArray(modelFieldValue)) {
			this.model[modelFieldName] = modelFieldValue.map(d => d instanceof descriptorClass ? d.model : d);
			return true;
		}
		const size = this.model[modelFieldName].length;
		if (modelFieldValue instanceof descriptorClass) {
			this.model[modelFieldName].push(modelFieldValue.valueOf());
			newItemAddedSuccessCallback(modelFieldValue);
		} else if (typeof modelFieldValue === 'object') {
			this.model[modelFieldName].push(modelFieldValue);
			newItemAddedSuccessCallback(new descriptorClass(modelFieldValue, this));
		} else {
			throw new ReferenceError(`expect object to be either an Array, ${descriptorClass.name} or JSON object`);
		}
		return size !== this.model[modelFieldName].length;
	}

	removeModelListItem(propertyName, child) {
		// ensure child is a DescriptorModel instance
		child = this.findChildByUid(child) || child;
		if (!child) {
			return false;
		}
		this.removeChild(child);
		const uid = child.uid;
		const length = this[propertyName].length;
		this[propertyName] = this[propertyName].filter(d => d.uid !== uid);
		return length !== this[propertyName].length;
	}

}
