/**
 * Created by onvelocity on 2/11/16.
 */
'use strict';

import d3 from 'd3'
import ColorGroups from '../ColorGroups'
import PathBuilder from '../graph/PathBuilder'
import DescriptorModelFactory from '../model/DescriptorModelFactory'
import DescriptorGraphSelection from './DescriptorGraphSelection'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import HighlightRecordServicePaths from './HighlightRecordServicePaths'
import ComposerAppActions from '../../actions/ComposerAppActions'

import '../../styles/GraphDescriptorModel.scss'

const iconTitles = {
	'vnffgd': 'fg',
	'constituent-vnfd': 'vnf',
	'nsd': 'ns',
	'vnfd': 'vnf',
	'vld': 'vl',
	'internal-vld': 'vl'
};

export default class GraphDescriptorModel {

	constructor(graph, classType, props = {}) {
		this.props = {};
		this.graph = graph;
		this.classType = classType;
		this.containers = [];
		this._dragHandler = () => {};
		if (!classType) {
			throw TypeError('Expect class type to be provided. Did you forget to pass in a class type, for example, DescriptorModelFactory.VirtualNetworkFunction?');
		}
		const defaults = {type: classType.type, className: classType.className, selector: [classType.className, classType.type, 'descriptor']};
		this.props = Object.assign(this.props, defaults, {descriptorBorderRadius: 20, descriptorWidth: 250, descriptorHeight: 55}, props);
		GraphDescriptorModel.buildRef(classType.type, this.graph.defs);
	}

	addContainers(containers) {
		this.containers = containers.filter(d => d instanceof this.classType);
	}

	get dragHandler() {
		return this._dragHandler;
	}

	set dragHandler(drag) {
		this._dragHandler = drag;
	}

	render() {

		const g = this.graph.containersGroup;

		const descriptor = g.selectAll('.' + this.props.selector.join('.')).data(this.containers, DescriptorModelFactory.containerIdentity);

		const path = new PathBuilder();

		const descriptorPath = PathBuilder.descriptorPath(this.props.descriptorBorderRadius, this.props.descriptorHeight, this.props.descriptorWidth).toString();

		// ENTER
		const box = descriptor.enter().append('g')
			.attr({
				'data-uid': d => d.uid,
				'data-key': d => d.key,
				'class': d => {
					return this.props.selector.join(' ');
				}
			}).on('cut', (container) => {

				let success = false;

				if (container && container.remove) {
					success = container.remove();
				}

				if (success) {
					CatalogItemsActions.catalogItemDescriptorChanged(container.getRoot());
				} else {
					d3.event.preventDefault();
				}

				d3.event.stopPropagation();

			}).call(this.dragHandler);

		box.append('path')
			.attr({
				class: d => d.type + '-descriptor-model-background-layer background-layer',
				d: descriptorPath,
				fill: ColorGroups.common.background,
				stroke: ColorGroups.common.background,
				'stroke-width': '2px'
			});

		box.append('path')
			.attr({
				class: d => d.type + '-descriptor-model-background background',
				d: descriptorPath,
				fill: d => `url(#${d.type}-descriptor-model-badge-fill)`,
				stroke: 'transparent',
				'stroke-width': '2px'
			});

		box.append('path')
			.attr({
				class: d => d.type + '-descriptor-model-border border',
				d: descriptorPath,
				fill: 'transparent',
				stroke: d => ColorGroups.getColorPairForType(d.type).secondary,
				'stroke-width': 1.25
			}).style({opacity: 0.75});

		box.append('path').attr({
			class: d => d.type + '-icon icon',
			d: d => d.icon.d,
			transform: d => `translate(${d.icon.translate ? d.icon.translate : '-4, 2'}) rotate(0) scale(${24 / d.icon.width}, ${24 / d.icon.width})`,
			fill: 'white',
			stroke: 'white'
		});

		box.append('text')
			.attr({
				class:d => d.type + '-type type',
				'font-weight': 100,
				'font-size': '12px',
				'text-anchor': 'middle',
				'text-transform': 'uppercase',
				fill: 'white',
				stroke: 'white',
				x: 13,
				y: 49
			}).text(d => iconTitles[d.type] || d.type).style({opacity: 1});

		box.append('text')
			.attr({
				class: d => d.type + '-title title',
				'font-weight': 100,
				'font-size': '12px',
				'text-anchor': 'middle',
				x: (d) => {
					const left = 0;
					const widthOffset = (d.position.width / 2) + 20;
					return left + widthOffset;
				},
				y: (d) => {
					const top = 0;
					const height = d.position.height;
					const heightOffset = height / 2;
					const textHeightOffset = 12 / 2;
					return top + heightOffset + textHeightOffset;
				}
			});

		box.each(function (d) {
			if (DescriptorModelFactory.isForwardingGraph(d)) {
				const box = d3.select(this);

				d.rsp.forEach((rsp, i) => {
					const colorLegend = box.append('g').attr({
						class:d => d.type + '-rsp-color-legend color-legend',
						transform: d => {
							const widthOffset = d.position.width - 20 - (i * 26);
							const heightOffset = d.position.height - 10;
							return `translate(${widthOffset}, ${heightOffset})`;
						}
					});
					colorLegend.append('ellipse').classed('colors', true).attr({
						rx: 12,
						ry: 7.5,
						fill: d => d.colors && d.colors.primary,
						stroke: d => d.colors && d.colors.secondary
					}).on('mouseenter', function (d) {
						HighlightRecordServicePaths.highlightPaths(rsp);
					}).on('mouseout', function () {
						HighlightRecordServicePaths.removeHighlighting();
					}).on('mouseleave', function () {
						HighlightRecordServicePaths.removeHighlighting();
					}).on('click', function () {
						d3.event.preventDefault();
						ComposerAppActions.selectModel(rsp);
					});
				});

			}
		});


		// ENTER & UPDATE
		descriptor.attr({
			transform: d => {
				const x = d.position.left;
				const y = d.position.top;
				return 'translate(' + x + ', ' + y + ')';
			}
		});

		descriptor.select('text.title')
			.text(d => {
				return d.title;
			});

		// EXIT
		descriptor.exit()
			.remove();

	}

	static buildRef(type, defs) {
		if (defs.selectAll('#' + type + '-descriptor-model-badge-fill')[0].length === 0) {
			var vld = defs.append('pattern').attr({
				id: type + '-descriptor-model-badge-fill',
				//patternUnits: 'userSpaceOnUse',
				patternContentUnits: 'objectBoundingBox',
				width: 1,
				height: 1,
				viewBox: '0 0 1 1',
				preserveAspectRatio: 'none'
			});
			vld.append('rect').attr({
				width: 1,
				height: 1,
				fill: 'white'//ensure transparent layers keep their color
			});
			vld.append('rect').attr({
				width: 1,
				height: 1,
				fill: ColorGroups.getColorPairForType(type).secondary
			}).style({opacity: 1});
			vld.append('rect').attr({
				y: 0,
				x: 0.22,
				width: 1,
				height: 1,
				fill: ColorGroups.common.background
			});
		}
	}

}
