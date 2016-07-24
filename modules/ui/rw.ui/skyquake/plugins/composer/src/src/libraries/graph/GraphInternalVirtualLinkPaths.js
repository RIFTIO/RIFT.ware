/**
 * Created by onvelocity on 2/15/16.
 */

import math from './math'
import PathBuilder from './PathBuilder'
import ColorGroups from '../ColorGroups'
import DescriptorModelFactory from '../model/DescriptorModelFactory'

/**
 *
 * This class draws the paths between the VLD and the Connection Point it references.
 *
 */

const line = d3.svg.line()
	.x(d => {
		return d.x;
	})
	.y(d => {
		return d.y;
	});

export default class GraphVirtualLinkPaths {

	constructor(graph, props) {
		this.graph = graph;
		this.props = props;
		this.maps = new WeakMap();
		this.maps.set(this, {vld: {}, containers: new Set()});
	}

	findVldForInternalConnectionPoint(icp) {
		const id = icp.vldRef;
		return this.maps.get(this).vld[id];
	}

	mapVld(vl) {
		this.maps.get(this).vld[vl.id] = vl;
	}

	get containersList() {
		return Array.from(this.maps.get(this).containers);
	}

	addContainer(container) {
		this.maps.get(this).containers.add(container);
	}

	addContainers(containers) {
		containers.filter(container => {
			if (DescriptorModelFactory.isInternalConnectionPoint(container)) {
				this.addContainer(container);
			} else if (DescriptorModelFactory.isInternalVirtualLink(container)) {
				this.mapVld(container);
			}
		});
	}

	renderPath(icp, ivld) {
		const path = line.interpolate('basis');
		const srcPoint = icp.position.centerPoint();
		const dstPoint = ivld.position.centerPoint();
		const srcIsTopMounted = /top/.test(icp.location);
		const srcSpline1 = {
			x: srcPoint.x,
			y: (srcIsTopMounted ? icp.position.top - 15 : icp.position.bottom + 15)
		};
		const srcSpline2 = {
			x: srcPoint.x,
			y: (srcIsTopMounted ? icp.position.top - 15 : icp.position.bottom + 15)
		};
		return path([srcPoint, srcSpline1, srcSpline2, dstPoint]);
	}

	render() {

		const paths = this.graph.paths.selectAll('.' + this.props.selector.join('.')).data(this.containersList, DescriptorModelFactory.containerIdentity);

		paths.enter().append('path');

		paths.attr({
			'data-uid': d => {
				return d.uid;
			},
			'class': d => {
				return 'connection between-' + d.parent.type + '-and-' + d.type;
			},
			'stroke-width': 5,
			stroke: ColorGroups.vld.primary,
			fill: 'transparent',
			d: d => {
				return this.renderPath(d, this.findVldForInternalConnectionPoint(d));
			}
		});

		paths.exit().remove();

	}

}
