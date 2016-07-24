/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 2/8/16.
 */
/*global describe, beforeEach, it, expect, xit */

'use strict';
import _ from 'lodash'
import DescriptorModel from 'libraries/model/DescriptorModel'

class TestDescriptorModel extends DescriptorModel {
	constructor(model, parent) {
		super(model, parent);
	}
}
describe('DescriptorModel', () => {
	let parent, child1, child2;
	beforeEach(() => {
		parent = new TestDescriptorModel({name: 'parent 1'});
		child1 = new TestDescriptorModel({name: 'child 1'}, parent);
		child2 = new TestDescriptorModel({name: 'child 2'}, parent);
	});
	it('creates a uid and stores it in the uiState field ":uid"', () => {
		expect(child1.uid).toEqual(child1.model.uiState[':uid']);
	});
	describe('.constructor(model, parent)', () => {
		it('creates child when parent is given on constructor', () => {
			const child = new TestDescriptorModel({name: 'test1'}, parent);
			expect(parent.children).toContain(child);
		});
	});
	describe('.findChildByUid', () => {
		it('finds an existing child', () => {
			const result = parent.findChildByUid(child2.uid);
			expect(result).toBe(child2);
		});
	});
});