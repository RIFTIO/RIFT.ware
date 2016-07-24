/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 11/23/15.
 */

'use strict';

import _ from 'lodash'
import ColorGroups from '../../ColorGroups'
import DescriptorModel from '../DescriptorModel'
import ForwardingGraph from './ForwardingGraph'
import VirtualLink from './VirtualLink'
import ConstituentVnfd from './ConstituentVnfd'
import PhysicalNetworkFunction from './PhysicalNetworkFunction'
import DescriptorModelFactory from '../DescriptorModelFactory'
import DescriptorModelMetaFactory from '../DescriptorModelMetaFactory'

const indexAsInteger = d => 1 + (parseInt(d.model['member-vnf-index'], 10) || 0);

const suffixAsInteger = function (field) {
	return (d) => 1 + (parseInt(String(d.model[field]).split('').reverse().join(''), 10) || 0);
};

const toBiggestValue = (newIndex, curIndex) => Math.max(newIndex, curIndex);

export default class NetworkService extends DescriptorModel {

	static get type() {
		return 'nsd';
	}

	static get className() {
		return 'NetworkService';
	}

	constructor(model, parent) {
		super(model, parent);
		this.type = 'nsd';
		this.className = 'NetworkService';
		this._connectors = [];
	}

	get constituentVnfd() {
		if (!this.model['constituent-vnfd']) {
			this.model['constituent-vnfd'] = [];
		}
		return this.model['constituent-vnfd'].map(d => DescriptorModelFactory.newConstituentVnfd(d, this));
	}

	set constituentVnfd(obj) {
		const updateNextVnfdIndex = (cvnfd) => {
			const items = this.constituentVnfd;
			const length = items.length;
			// the default value is set to an instance count but we want it to be a sequence no
			cvnfd.model['member-vnf-index'] = 0;
			cvnfd.model['member-vnf-index'] = items.map(indexAsInteger).reduce(toBiggestValue, length);
		};
		this.updateModelList('constituent-vnfd', obj, ConstituentVnfd, updateNextVnfdIndex);
	}

	createConstituentVnfd(model) {
		model = model || DescriptorModelMetaFactory.createModelInstanceForType('nsd.constituent-vnfd');
		return this.constituentVnfd = DescriptorModelFactory.newConstituentVnfd(model, this);
	}

	removeConstituentVnfd(cvnfd) {
		cvnfd = this.findChildByUid(cvnfd);
		this.vld.forEach(vld => vld.removeVnfdConnectionPointRefForVnfdIndex(cvnfd.vnfdIndex));
		this.vnffgd.forEach(fg => fg.removeVnfdConnectionPointRefForVnfdIndex(cvnfd.vnfdIndex));
		return this.removeModelListItem('constituentVnfd', cvnfd);
	}

	get pnfd() {
		if (!this.model.pnfd) {
			this.model.pnfd = [];
		}
		return this.model.pnfd.map(d => DescriptorModelFactory.newPhysicalNetworkFunction(d, this));
	}

	set pnfd(obj) {
		this.updateModelList('pnfd', obj, PhysicalNetworkFunction);
	}

	createPnfd(model) {
		model = model || DescriptorModelMetaFactory.createModelInstanceForType('nsd.pnfd');
		return this.pnfd = DescriptorModelFactory.newPhysicalNetworkFunction(model, this);
	}

	removePnfd(pnfd) {
		return this.removeModelListItem('pnfd', pnfd);
	}


	get vld() {
		if (!this.model.vld) {
			this.model.vld = [];
		}
		return this.model.vld.map(d => DescriptorModelFactory.newVirtualLink(d, this));
	}

	set vld(obj) {
		this.updateModelList('vld', obj, VirtualLink);
	}

	createVld() {
		const model = DescriptorModelMetaFactory.createModelInstanceForType('nsd.vld');
		return this.vld = DescriptorModelFactory.newVirtualLink(model, this);
	}

	removeVLD(vld) {
		return this.removeModelListItem('vld', vld);
	}


	get vnffgd() {
		if (!this.model.vnffgd) {
			this.model.vnffgd = [];
		}
		return this.model.vnffgd.map(d => DescriptorModelFactory.newForwardingGraph(d, this)).map((fg, i) => {
			fg.uiState.colors = ColorGroups.getColorPair(i);
			return fg;
		});
	}

	set vnffgd(obj) {
		const onAddForwardingGraph = (fg) => {
			const index = this.vnffgd.map(suffixAsInteger('short-name')).reduce(toBiggestValue, this.vnffgd.length);
			fg.model['short-name'] = 'FG-' + index;
		};
		this.updateModelList('vnffgd', obj, ForwardingGraph, onAddForwardingGraph);
	}

	createVnffgd(model) {
		model = model || DescriptorModelMetaFactory.createModelInstanceForType('nsd.vnffgd');
		return this.vnffgd = DescriptorModelFactory.newForwardingGraph(model, this);
	}

	removeVnffgd(fg) {
		return this.removeModelListItem('vnffgd', fg);
	}

	get forwardingGraphs() {
		return this.vnffgd;
	}


	// NOTE temporarily disable NSD connection points
	// https://trello.com/c/crVgg2A1/88-do-not-render-nsd-connection-in-the-composer
	//get connectionPoint() {
	//	if (this.model && this.model['connection-point']) {
	//		return this.model['connection-point'];
	//	}
	//	return [];
	//}
	//
	//get connectors() {
	//	const parent = this;
	//	if (!this._connectors.length) {
	//		this._connectors = this.connectionPoint.map(cp => {
	//			return DescriptorModelFactory.newConnectionPoint(parent, cp);
	//		});
	//	}
	//	return this._connectors;
	//}

	removeAnyConnectionsForConnector(cpc) {
		// todo need to also remove connection points from related ForwardingGraph paths
		this.vld.forEach(vldc => vldc.removeVnfdConnectionPointRefKey(cpc.key));
	}

	createConstituentVnfdForVnfd(vnfdRef) {
		const cvnfd = this.createConstituentVnfd();
		cvnfd.vnfdRef = vnfdRef;
		cvnfd.vnfdId = vnfdRef.id;
		return cvnfd;
	}

}
