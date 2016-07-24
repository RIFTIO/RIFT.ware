
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/24/15.
 */
'use strict';

import _ from 'lodash'
import d3 from 'd3'
import math from './math'
import ClassNames from 'classnames'
import ColorGroups from '../ColorGroups'
import DescriptorModel from './../model/DescriptorModel'
import DescriptorModelFactory from './../model/DescriptorModelFactory'
import DescriptorGraphSelection from './DescriptorGraphSelection'
import DescriptorGraphEdgeBuilder from './DescriptorGraphPathBuilder'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import DescriptorGraphGrid from './DescriptorGraphGrid'
import SelectionManager from './../SelectionManager'
import GraphConnectionPointNumber from './GraphConnectionPointNumber'
import RelationsAndNetworksLayout from './layouts/RelationsAndNetworksLayout'

const defaults = {
	width: 1000,
	height: 500,
	padding: 10,
	snapTo: 15,
	zoom: 100,
	connectionPointSize: 135
};

export default class DescriptorGraph {

	constructor(element, props) {

		this.element = element;
		this.props = Object.assign({}, props, defaults);
		this.layouts = [RelationsAndNetworksLayout];
		this.containers = [];
		this.showMoreInfo = false;
		this.scale = 1;

		if (!element) {
			throw new ReferenceError('An HTML DOM element is required to render DescriptorGraph.');
		}

		this.svg = d3.select(element).append('svg')
			.attr('class', 'DescriptorGraph')
			.attr('width', this.props.width)
			.attr('height', this.props.height)
			.attr('data-offset-parent', true);

		this.g = this.svg.append('g')
			.attr('class', 'viewport');

		// set the scale of everything
		this.zoom(this.props.zoom || defaults.zoom);

		this.defs = this.svg.append('defs');
		this.grid = this.g.append('g').classed('grid', true);
		this.paths = this.g.append('g').classed('paths', true);
		this.connectorsGroup = this.g.append('g').classed('connectors', true);
		this.containersGroup = this.g.append('g').classed('containers', true);
		this.cpNumbers = this.g.append('g').classed('connection-point-numbers', true);
		this.forwardingGraphPaths = this.g.append('g').classed('forwarding-graph-paths', true);
		this.selectionOutlineGroup = this.g.append('g').attr('data-outline-indicator', true);

		this.defs.append('marker')
			.attr({
				id: 'relation-marker-end',
				'markerWidth': 30,
				'markerHeight': 30,
				'orient': '0',
				//markerUnits: 'userSpaceOnUse'
				viewBox: '-15 -15 30 30'
			})
			.append('path')
			.attr({
				d: d3.svg.symbol().type('circle'),
				'class': 'relation-marker-end'
			});

		this.update();

	}

	zoom(zoom) {
		const scale = zoom / 100;
		if (this.scale !== scale) {
			this.scale = scale;
			const transform = 'scale(' + this.scale + ')';
			requestAnimationFrame(() => {
				this.g.attr('transform', transform);
			});
		}
	}

	get containerPositionMap() {
		if (this.containers[0].uiState.containerPositionMap) {
			return this.containers[0].uiState.containerPositionMap;
		} else {
			return this.containers[0].uiState.containerPositionMap = {};
		}
	}

	lookupSavedContainerPosition(container) {
		return this.containerPositionMap[container.key];
	}

	saveContainerPosition(container) {
		this.containerPositionMap[container.key] = container.position.value();
	}

	update() {

		const graph = this;

		const filterContainers = d => (graph.showMoreInfo ? true : d.type !== 'vdu');
		const containers = this.containers.filter(filterContainers);
		const layouts = graph.layouts.map(layoutFactory => layoutFactory.bind(this)());

		function runLayout(i) {
			const layout = layouts[i];
			if (layout) {
				layout.addContainers(containers);
				layout.render(graph, runLayout.bind(null, i + 1));
			}
		}
		runLayout(0);

		const selection = new DescriptorGraphSelection(graph);
		selection.addContainers(containers);
		selection.render();

		const edgeBuilder = new DescriptorGraphEdgeBuilder(graph);
		edgeBuilder.addContainers(containers);
		edgeBuilder.render();

		const grid = new DescriptorGraphGrid(graph, {size: defaults.snapTo, padding: defaults.padding});
		grid.render();

	}

	destroy() {
		if (this.svg) {
			this.svg.remove();
			delete this.svg;
		}
	}

}
