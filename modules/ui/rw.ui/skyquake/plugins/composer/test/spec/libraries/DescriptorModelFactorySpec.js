/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 2/8/16.
 */
/*global describe, beforeEach, it, expect, xit, xdescribe */

'use strict';
import _ from 'lodash'
import DescriptorModelSerializer from '../../../src/libraries/model/DescriptorModelSerializer'
import DescriptorModelFactory from '../../../src/libraries/model/DescriptorModelFactory'
import SampleCatalogs from 'json!../../../src/assets/ping-pong-catalog.json'
import TestCatalogs from 'json!../../helpers/test-clean-input-output-model.json'

describe('DescriptorModelFactory', () => {
	it('exists', () => {
		expect(DescriptorModelFactory).toBeDefined();
	});
	describe('buildCatalogItemFactory', () => {
		let containers;
		beforeEach(() => {
			const nsdJson = _.cloneDeep(SampleCatalogs[0].descriptors[0]);
			// the CatalogItemsStore adds the type to the uiState field when the catalog is loaded
			nsdJson.uiState = {type: 'nsd'};
			// the user will open a catalog item by dbl clicking on it in the ui that is when we
			// parse the item into a DescriptorModel class instance as follows....
			const factory = DescriptorModelFactory.buildCatalogItemFactory(SampleCatalogs);
			// the result is a list of all the containers defined with then NSD JSON data
			containers = [nsdJson].reduce(factory, []);
		});
		it('ignores an empty object', () => {
			const factory = DescriptorModelFactory.buildCatalogItemFactory([]);
			const result = [{}].reduce(factory, []);
			expect(result).toEqual([]);
		});
		it('parses an NSD object', () => {
			const nsdJson = _.cloneDeep(SampleCatalogs[0].descriptors[0]);
			nsdJson.uiState = {type: 'nsd'};
			const factory = DescriptorModelFactory.buildCatalogItemFactory(SampleCatalogs);
			const result = [nsdJson].reduce(factory, [])[0];
			expect(result.id).toEqual('ba1dfbcc-626b-11e5-998d-6cb3113b406f');
		});
		it('parses the constituent-vnfd classes', () => {
			const nsd = containers[0];
			const cvnfd = containers.filter(d => DescriptorModelFactory.isConstituentVnfd(d));
			expect(nsd.constituentVnfd).toEqual(cvnfd);
		});
		describe('ConstituentVnfd', () => {
			it('connection-points derive from referenced VNFD', () => {
				const constituentVNFDs = containers.filter(d => DescriptorModelFactory.isConstituentVnfd(d)).map(d => d.vnfdId);
				expect(constituentVNFDs).toEqual(['ba145e82-626b-11e5-998d-6cb3113b406f', 'ba1947da-626b-11e5-998d-6cb3113b406f']);
			});
		});
		describe('DescriptorModelSerializer', () => {
			it('outputs the same JSON that was parsed by the .buildCatalogItemFactory method', () => {
				const inputJSON = _.cloneDeep(TestCatalogs[0].descriptors[0]);
				inputJSON.uiState = {type: 'nsd'};
				const factory = DescriptorModelFactory.buildCatalogItemFactory(TestCatalogs);
				const parsedModel = [inputJSON].reduce(factory, []);
				const serialized = DescriptorModelSerializer.serialize(parsedModel[0].model);
				const expectedSerializedString = '{"id":"ba1dfbcc-626b-11e5-998d-6cb3113b406f","name":"ping-pong-nsd","short-name":"ping-pong-nsd","vendor":"RIFT.io","logo":"rift.png","description":"Toy NS","version":"1.0","connection-point":[{"name":"ping-pong-nsd/cp0"},{"name":"ping-pong-nsd/cp1"}],"vld":[{"id":"ba1c03a8-626b-11e5-998d-6cb3113b406f","name":"ping-pong-vld","short-name":"ping-pong-vld","vendor":"RIFT.io","description":"Toy VL","version":"1.0","type":"ELAN","vnfd-connection-point-ref":[{"member-vnf-index-ref":1,"vnfd-id-ref":"ba145e82-626b-11e5-998d-6cb3113b406f","vnfd-connection-point-ref":"ping-vnfd/cp1"},{"member-vnf-index-ref":2,"vnfd-id-ref":"ba1947da-626b-11e5-998d-6cb3113b406f","vnfd-connection-point-ref":"pong-vnfd/cp1"}],"provider-network":{"name":"physnet1","overlay-type":"VLAN"}}],"constituent-vnfd":[{"member-vnf-index":1,"vnfd-id-ref":"ba145e82-626b-11e5-998d-6cb3113b406f"},{"member-vnf-index":2,"vnfd-id-ref":"ba1947da-626b-11e5-998d-6cb3113b406f"}],"vnffgd":[{"id":"1d6382bb-52fa-43b6-9489-d764a0a27da9","name":"vnffgd-5","short-name":"FG-1","rsp":[{"id":"a10b372d-19a1-4d84-a246-82bfceddae12","name":"rsp-6","vnfd-connection-point-ref":[{"vnfd-id-ref":"ba145e82-626b-11e5-998d-6cb3113b406f","member-vnf-index-ref":1,"vnfd-connection-point-ref":"ping-vnfd/cp1"},{"vnfd-id-ref":"ba1947da-626b-11e5-998d-6cb3113b406f","member-vnf-index-ref":2,"vnfd-connection-point-ref":"pong-vnfd/cp0"}]}],"classifier":[{"id":"1-a5b8-45b5-8163-f2577555d561","name":"classifier-1","rsp-id-ref":"a10b372d-19a1-4d84-a246-82bfceddae12","match-attributes":[{"id":"1","ip-proto":"123.0.0.1","source-ip-address":"10.4.0.1","destination-ip-address":"10.4.0.2","source-port":"1234","destination-port":"4321"}]},{"id":"2-a5b8-45b5-8163-f2577555d561","name":"classifier-2","rsp-id-ref":"a10b372d-19a1-4d84-a246-82bfceddae12","match-attributes":[{"id":"1","ip-proto":"123.0.0.1","source-ip-address":"10.4.0.1","destination-ip-address":"10.4.0.2","source-port":"1234","destination-port":"4321"}]},{"id":"3-a5b8-45b5-8163-f2577555d561","name":"classifier-3","rsp-id-ref":"a10b372d-19a1-4d84-a246-82bfceddae12","match-attributes":[{"id":"1","ip-proto":"123.0.0.1","source-ip-address":"10.4.0.1","destination-ip-address":"10.4.0.2","source-port":"1234","destination-port":"4321"}]},{"id":"4-a5b8-45b5-8163-f2577555d561","name":"classifier-4","rsp-id-ref":"a10b372d-19a1-4d84-a246-82bfceddae12","match-attributes":[{"id":"1","ip-proto":"123.0.0.1","source-ip-address":"10.4.0.1","destination-ip-address":"10.4.0.2","source-port":"1234","destination-port":"4321"}]}]}]}';
				const result = JSON.stringify(serialized);
				expect(expectedSerializedString).toEqual(result);
			});
		});
	});
});
