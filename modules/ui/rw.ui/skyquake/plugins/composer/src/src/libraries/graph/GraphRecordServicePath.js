/**
 * Created by onvelocity on 2/10/16.
 */

import math from './math'
import PathBuilder from './PathBuilder'
import DescriptorModelFactory from '../model/DescriptorModelFactory'

import '../../styles/GraphRecordServicePaths.scss'

export default class GraphRecordServicePath {

	constructor(graph, props) {
		this.graph = graph;
		this.props = props;
		this.containers = [];
		this.networkMap = new WeakMap();
		this.networkMap.set(this, {networkMap: {}});

	}

	getNetwork(key) {
		return this.networkMap.get(this).networkMap[key];
	}

	putNetwork(item) {
		this.networkMap.get(this).networkMap[item.key + item.type] = item;
	}

	addContainers(containers) {

		containers.filter(d => {
			if (DescriptorModelFactory.isRecordServicePath(d)) {
				return true;
			}
			if (DescriptorModelFactory.isRspConnectionPointRef(d)) {
				return false;
			}
			if (DescriptorModelFactory.isConnectionPoint(d)) {
				this.putNetwork(d);
			}
			if (DescriptorModelFactory.isVnfdConnectionPointRef(d)) {
				this.putNetwork(d)
			}
		}).forEach(d => this.containers.push(d));

	}

	render() {

		const pathsGroup = this.graph.forwardingGraphPaths;

		const path = pathsGroup.selectAll('.rsp-path').data(this.containers, DescriptorModelFactory.containerIdentity);

		path.enter().append('path');

		path.attr({
			'class': 'rsp-path path',
			'stroke': rsp => rsp.parent.colors.primary,
			'data-id': d => d.id,
			'stroke-width': 1.5,
			fill: 'transparent',
			'shape-rendering': 'geometricPrecision',
			d: (rsp, rspIndex) => {

				if (!rsp.uiState.showPath) {
					return '';
				}

				const cpList = rsp.connectionPoints.filter(d => d);
				const cpCount = rsp.connectionPoints.length - 1;

				const path = new PathBuilder();

				const pathOffset = 20 * rspIndex;

				return rsp.connectionPoints.reduce((r, cpRef, cpRefIndex) => {

					const cp = this.getNetwork(cpRef.key + 'connection-point');
					const point = cp.position.centerPoint();
					const vnfdCpRef = this.getNetwork(cp.key + 'vnfd-connection-point-ref');

					if (path.length === 0) {
						path.M(point.x, point.y, {container: cp});
					} else if (cpRefIndex === 1) {

						const last = path.last();
						//
						//const previousContainer = path.findPrevious(d => d.info.container);
						//
						//let distance = 200;

						//if (previousContainer) {
						//	distance = math.distanceBetweenPositions(previousContainer.info.container.position, cp.position);
						//}

						if (last.cmd === 'M') {
							path.C(last.x + 100 + pathOffset, last.y - 100 - pathOffset, point.x - 100 - pathOffset, point.y - 100 - pathOffset, point.x, point.y);
						} else {
							path.S(point.x - 100, point.y - 100, point.x, point.y);
						}

					} else {

						//const last = path.last();
						//
						//const previousContainer = path.findPrevious(d => d.info.container);
						//
						//let distance = 200;

						//if (previousContainer) {
						//	distance = math.distanceBetweenPositions(previousContainer.info.container.position, cp.position);
						//}

						path.S(point.x - 100 - pathOffset, point.y - 100 - pathOffset, point.x, point.y);

					}


					if (cpRefIndex < cpCount && vnfdCpRef) {

						const nextVnfdCpRef = this.getNetwork(cpList[cpRefIndex + 1].key + 'vnfd-connection-point-ref');

						if (nextVnfdCpRef) {

							const cpsOnSameVnfd = nextVnfdCpRef && nextVnfdCpRef.key === vnfdCpRef.key;
							const cpsOnSameVld = nextVnfdCpRef.parent.key === vnfdCpRef.parent.key;

							if (cpsOnSameVld) {

								const vldCenter = vnfdCpRef.parent.position.centerPoint();

								const last = path.last();

								if (last.cmd === 'M') {
									path.C(last.x, last.y + 40 + pathOffset, vldCenter.x, vldCenter.y - 40 - pathOffset, vldCenter.x, vldCenter.y);
								} else {
									path.S(vldCenter.x, vldCenter.y - 40 - pathOffset, vldCenter.x, vldCenter.y);
								}

								//const distance = math.distanceBetweenPositions(cp.position, vnfdCpRef.parent.position);

							} else if (cpsOnSameVnfd) {

								const last = path.last();

								const vldCenter = vnfdCpRef.parent.position.centerPoint();

								if (last.cmd === 'M') {
									path.C(last.x, last.y + 40, vldCenter.x, vldCenter.y - 40, vldCenter.x, vldCenter.y);
								} else {
									path.S(vldCenter.x, vldCenter.y - 40, vldCenter.x, vldCenter.y);
								}

							}

						}

					}

					return path.toString();

				}, '')
			}

		});

		path.exit().remove();

	}

}
