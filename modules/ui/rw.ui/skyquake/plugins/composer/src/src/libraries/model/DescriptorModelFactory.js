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
import d3 from 'd3'
import UID from './../UniqueId'
import guid from '../guid'
import Position from './../graph/Position'
import ColorGroups from '../ColorGroups'
import DescriptorModel from './DescriptorModel'
import DescriptorModelMetaFactory from './DescriptorModelMetaFactory'
import DescriptorModelMetaProperty from './DescriptorModelMetaProperty'

import Classifier from './descriptors/Classifier'
import ClassifierMatchAttributes from './descriptors/ClassifierMatchAttributes'
import ConnectionPoint from './descriptors/ConnectionPoint'
import VnfdConnectionPointRef from './descriptors/VnfdConnectionPointRef'
import ConstituentVnfd from './descriptors/ConstituentVnfd'
import ConstituentVnfdConnectionPoint from './descriptors/ConstituentVnfdConnectionPoint'
import ForwardingGraph from './descriptors/ForwardingGraph'
import InternalConnectionPoint from './descriptors/InternalConnectionPoint'
import InternalVirtualLink from './descriptors/InternalVirtualLink'
import NetworkService from './descriptors/NetworkService'
import PhysicalNetworkFunction from './descriptors/PhysicalNetworkFunction'
import RecordServicePath from './descriptors/RecordServicePath'
import RspConnectionPointRef from './descriptors/RspConnectionPointRef'
import VirtualDeploymentUnit from './descriptors/VirtualDeploymentUnit'
import VirtualLink from './descriptors/VirtualLink'
import VirtualNetworkFunction from './descriptors/VirtualNetworkFunction'
import VirtualNetworkFunctionReadOnlyWrapper from './descriptors/VirtualNetworkFunctionReadOnlyWrapper'
import InternalConnectionPointRef from './descriptors/InternalConnectionPointRef'
import VirtualNetworkFunctionConnectionPoint from './descriptors/VirtualNetworkFunctionConnectionPoint'
import VirtualDeploymentUnitInternalConnectionPoint from './descriptors/VirtualDeploymentUnitInternalConnectionPoint'

function findChildDescriptorModelAndUpdateModel(model, parent) {
	if (parent instanceof DescriptorModel) {
		const child = parent.findChildByUid(model);
		if (child) {
			child.model = model;
			return child;
		}
	}
}

let identity = 0;

class DescriptorModelFactory {


	static containerIdentity(d) {
		const parentId = UID.from(d && d.parent);
		// note the incremental counter is to always force d3 models to update
		return (parentId ? parentId + ':' : '') + UID.from(d) + ':' + identity++;
	}

	/**
	 * Create a reduce function that creates a DescriptorModel element tree
	 * representation of a Catalog Item and then returns a flat list of
	 * DescriptorModel elements. The first item in the array is root.

	 * @returns {Function}
	 */
	static buildCatalogItemFactory(catalogs) {

		function findCatalogItemByTypeAndId(type, id) {
			return catalogs.filter(catalog => catalog.type === type).reduce((a, b) => a.concat(b.descriptors), []).filter(d => d.id === id)[0];
		}

		function mapVLD(vld, containerList) {
			containerList.push(vld);
			vld.connection.forEach(d => containerList.push(d));
		}

		function mapIVLD(ivld, containerList) {
			containerList.push(ivld);
			ivld.connection.forEach(d => containerList.push(d));
		}

		function mapRspConnectionPoint(cp, containerList) {
			containerList.push(cp);
		}

		function mapClassifier(classifier, containerList) {
			containerList.push(classifier);
			// get the referenced vnfd required for rendering the connection point
			const vnfdRef = findCatalogItemByTypeAndId('vnfd', classifier.model['vnfd-id-ref']);
			if (vnfdRef) {
				classifier.uiState.vnfdRef = vnfdRef;
			}
			classifier.matchAttributes.forEach(attr => containerList.push(attr));
		}

		function mapRSP(rsp, containerList) {
			containerList.push(rsp);
			rsp.vnfdConnectionPointRef.forEach(cpRef => mapRspConnectionPoint(cpRef, containerList));
		}

		function mapFG(fg, containerList) {
			containerList.push(fg);
			fg.rsp.forEach(rsp => mapRSP(rsp, containerList));
			fg.classifier.forEach(classifier => mapClassifier(classifier, containerList));
		}

		function mapVDU(vdu, containerList) {
			containerList.push(vdu);
			vdu.connectionPoint.forEach(d => containerList.push(d));
		}

		function mapCVNFD(cvnfd, containerList) {
			// get the referenced vnfd required for rendering the connection points
			const vnfdRef = findCatalogItemByTypeAndId('vnfd', cvnfd.model['vnfd-id-ref']);
			if (!vnfdRef) {
				console.warn('No VNFD found in catalog with id: ' + cvnfd['vnfd-id-ref']);
			}
			cvnfd.vnfdRef = vnfdRef;
			containerList.push(cvnfd);
			cvnfd.vld.forEach(vld => mapIVLD(vld, containerList));
			cvnfd.connectionPoint.forEach(cp => containerList.push(cp));
		}

		function mapNSD(nsd, containerList) {
			containerList.push(nsd);
			nsd.constituentVnfd.forEach(cvnfd => mapCVNFD(cvnfd, containerList));
			nsd.vld.forEach(vld => mapVLD(vld, containerList));
			nsd.vnffgd.forEach(fg => mapFG(fg, containerList));
		}

		function mapVNFD(vnfd, containerList) {
			containerList.push(vnfd);
			vnfd.vdu.forEach(vdu => mapVDU(vdu, containerList));
			vnfd.vld.forEach(vld => mapIVLD(vld, containerList));
			vnfd.connectionPoint.forEach(cp => containerList.push(cp));
		}

		function mapPNFD(pnfd, containerList) {
			containerList.push(pnfd);
		}

		return (containerList, obj) => {
			if (_.isEmpty(obj)) {
				return containerList;
			}
			switch (obj.uiState.type) {
			case 'nsd':
				mapNSD(DescriptorModelFactory.newNetworkService(obj), containerList);
				break;
			case 'vnfd':
				mapVNFD(DescriptorModelFactory.newVirtualNetworkFunction(obj), containerList);
				break;
			case 'pnfd':
				mapPNFD(DescriptorModelFactory.newPhysicalNetworkFunction(obj), containerList);
				break;
			default:
				throw new ReferenceError('only catalog items can be rendered in the canvas. unexpected type: ' + obj.uiState.type);
			}
			return containerList;
		};

	}

	static newNetworkService(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new NetworkService(model, parent);
	}

	static newConstituentVnfd(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new ConstituentVnfd(model, parent);
	}

	static newVirtualNetworkFunction(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VirtualNetworkFunction(model, parent);
	}

	static newVirtualNetworkFunctionConnectionPoint(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VirtualNetworkFunctionConnectionPoint(model, parent);
	}

	static newInternalConnectionPoint(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new InternalConnectionPoint(model, parent);
	}

	static newVirtualDeploymentUnit(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VirtualDeploymentUnit(model, parent);
	}

	static newVirtualDeploymentUnitInternalConnectionPoint(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VirtualDeploymentUnitInternalConnectionPoint(model, parent);
	}

	static newVirtualLink(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VirtualLink(model, parent);
	}

	static newInternalVirtualLink(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new InternalVirtualLink(model, parent);
	}

	static newPhysicalNetworkFunction(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new PhysicalNetworkFunction(model, parent);
	}

	static newConstituentVnfdConnectionPoint(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new ConstituentVnfdConnectionPoint(model, parent);
	}

	static newVnfdConnectionPointRef(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new VnfdConnectionPointRef(model, parent);
	}

	static newForwardingGraph(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new ForwardingGraph(model, parent);
	}

	static newRecordServicePath(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new RecordServicePath(model, parent);
	}

	static newRspConnectionPointRef(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new RspConnectionPointRef(model, parent);
	}

	static newVirtualNetworkFunctionReadOnlyWrapper(vnfdToWrap, parent) {
		let model;
		if (vnfdToWrap instanceof DescriptorModel) {
			if (vnfdToWrap instanceof VirtualNetworkFunction) {
				model = vnfdToWrap.model;
			} else {
				throw new ReferenceError(`expected a VirtualNetworkFunction but got a ${vnfdToWrap.className}`);
			}
		} else {
			model = vnfdToWrap;
		}
		return new VirtualNetworkFunctionReadOnlyWrapper(_.cloneDeep(model), parent);
	}

	static newClassifier(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new Classifier(model, parent);
	}

	static newClassifierMatchAttributes(model, parent) {
		return findChildDescriptorModelAndUpdateModel(model, parent) || new ClassifierMatchAttributes(model, parent);
	}

	static newInternalConnectionPointRef(model, parent) {
		// note do not find children bc model is not an object it is a leaf-list primative and so the class manages it
		return new InternalConnectionPointRef(model, parent);
	}

	/**
	 * Special instance of a RecordServicePath that will create its parent ForwardingGraph it does not exist.
	 *
	 * This is useful to present a stub RSP as an initiator to create Paths without explicitly creating an RSP - just
	 * start building a path and it auto-magically builds the RSP.
	 *
	 * @param parent
	 * @param model
	 */
	static newRecordServicePathFactory(model, parent) {
		return new AutoCreateRecordServicePath(model, parent);
	}

	static isContainer(obj) {
		return obj instanceof DescriptorModel;
	}

	static isConnector(obj) {
		return obj instanceof ConnectionPoint;
	}

	static isConnectionPoint(obj) {
		return obj instanceof ConnectionPoint;
	}

	static isConstituentVnfd(obj) {
		return obj instanceof ConstituentVnfd;
	}

	static isConstituentVnfdWithServiceChain(obj, chain) {
		return obj instanceof ConstituentVnfd && obj.vnfdServiceFunctionChain === chain;
	}

	static isNetworkService(obj) {
		return obj instanceof NetworkService;
	}

	static isVnfdConnectionPointRef(obj) {
		return obj instanceof VnfdConnectionPointRef;
	}

	static isRecordServicePath(obj) {
		return obj instanceof RecordServicePath;
	}

	static isRspConnectionPointRef(obj) {
		return obj instanceof RspConnectionPointRef;
	}

	static isVirtualLink(obj) {
		return obj instanceof VirtualLink;
	}

	static isVirtualNetworkFunction(obj) {
		return obj instanceof VirtualNetworkFunction;
	}

	static isForwardingGraph(obj) {
		return obj instanceof ForwardingGraph;
	}

	static isInternalConnectionPoint(obj) {
		return obj instanceof InternalConnectionPoint;
	}

	static isInternalVirtualLink(obj) {
		return obj instanceof InternalVirtualLink;
	}

	static get VirtualLink () {
		return VirtualLink;
	}

	static get NetworkService () {
		return NetworkService;
	}

	static get ForwardingGraph () {
		return ForwardingGraph;
	}

	static get VirtualNetworkFunction () {
		return VirtualNetworkFunction;
	}

	static get ConstituentVnfd () {
		return ConstituentVnfd;
	}

	static get Classifier() {
		return Classifier;
	}

	static get ClassifierMatchAttributes() {
		return ClassifierMatchAttributes;
	}

	static get VnfdConnectionPointRef() {
		return VnfdConnectionPointRef;
	}

	static get VirtualDeploymentUnit() {
		return VirtualDeploymentUnit;
	}

	static get InternalConnectionPoint() {
		return InternalConnectionPoint;
	}

	static get InternalVirtualLink() {
		return InternalVirtualLink;
	}

	static get InternalConnectionPointRef() {
		return InternalConnectionPointRef;
	}

	static get VirtualNetworkFunctionConnectionPoint() {
		return VirtualNetworkFunctionConnectionPoint;
	}

	static get VirtualDeploymentUnitInternalConnectionPoint() {
		return VirtualDeploymentUnitInternalConnectionPoint;
	}

}

/**
 * Auto create a RecordServicePath when a VnfdConnectionPointRef is added.
 */
class AutoCreateRecordServicePath extends RecordServicePath {

	constructor(model, parent) {
		super(model, null);
		this.parent = parent;
		this.isFactory = true;
	}

	create() {
		return this.parent.createRsp();
	}

	createVnfdConnectionPointRef(obj) {
		const rsp = this.create();
		rsp.createVnfdConnectionPointRef(obj);
		return rsp;
	}

}

export default DescriptorModelFactory;
