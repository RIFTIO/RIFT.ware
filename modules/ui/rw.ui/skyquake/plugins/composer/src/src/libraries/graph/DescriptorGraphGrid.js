
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 12/3/15.
 */

import d3 from 'd3'

const defaults = {
	size: 15,
	padding: 5
};

export default class DescriptorGraphGrid {

	constructor(graph, props) {
		this.graph = graph;
		Object.assign(props, defaults);
		this.size = props.size;
		this.padding = props.padding;
	}

	render() {

		const grid = this.graph.grid;
		const width = 2 * this.graph.svg.attr('width');
		const height = 2 * this.graph.svg.attr('height');
		const yAxis = d3.range(0, height, this.size);
		const xAxis = d3.range(0, width, this.size);

		grid.selectAll('line.vertical')
			.data(xAxis)
			.enter().append('svg:line')
			.classed('vertical', true)
			.attr('x1', (d) => d)
			.attr('y1', 0)
			.attr('x2', (d) => d)
			.attr('y2', height);

		grid.selectAll('line.horizontal')
			.data(yAxis)
			.enter().append('svg:line')
			.classed('horizontal', true)
			.attr('x1', 0)
			.attr('y1', (d) => d)
			.attr('x2', width)
			.attr('y2', (d) => d);

	}

}