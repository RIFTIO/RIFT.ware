/**
 * Created by onvelocity on 2/10/16.
 */
import alt from '../../../alt'
import _ from 'lodash'
import d3 from 'd3'
import math from '../math'
import ClassNames from 'classnames'
import ColorGroups from '../../ColorGroups'
import GraphVirtualLink from '../GraphVirtualLink'
import GraphNetworkService from '../GraphNetworkService'
import GraphForwardingGraph from '../GraphForwardingGraph'
import GraphConstituentVnfd from '../GraphConstituentVnfd'
import GraphVirtualNetworkFunction from '../GraphVirtualNetworkFunction'
import SelectionManager from '../../SelectionManager'
import GraphConnectionPointNumber from '../GraphConnectionPointNumber'
import CatalogItemsActions from '../../../actions/CatalogItemsActions'
import DescriptorModelFactory from '../../model/DescriptorModelFactory'
import DescriptorGraphSelection from '../DescriptorGraphSelection'
import GraphVirtualDeploymentUnit from '../GraphVirtualDeploymentUnit'
import GraphRecordServicePath from '../GraphRecordServicePath'
import GraphInternalVirtualLink from '../GraphInternalVirtualLink'
import TooltipManager from '../../TooltipManager'

function onCutDelegateToRemove(container) {
	function onCut(event) {
		event.target.removeEventListener('cut', onCut);
		if (container.remove()) {
			CatalogItemsActions.catalogItemDescriptorChanged.defer(container.getRoot());
		} else {
			event.preventDefault();
		}
	}
	this.addEventListener('cut', onCut);
}

export default function RelationsAndNetworksLayout() {

	const graph = this;
	const props = this.props;
	const containerWidth = 250;
	const containerHeight = 55;
	const marginTop = 20;
	const marginLeft = 10;
	const containerList = [];
	const leftOffset = containerWidth;

	const snapTo = (value) => {
		return Math.max(props.snapTo * Math.round(value / props.snapTo), props.padding);
	};

	const line = d3.svg.line()
		.x(d => {
			return d.x;
		})
		.y(d => {
			return d.y;
		});

	function countAncestors(container = {}) {
		let count = 0;
		while (container.parent) {
			count++;
			container = container.parent;
		}
		return count;
	}

	function renderRelationPath(src, dst) {
		const path = line.interpolate('basis');
		const srcPoint = src.position.centerPoint();
		const dstPoint = dst.position.centerPoint();
		const angle = math.angleBetweenPositions(src.position, dst.position);
		if (angle < 180) {
			srcPoint.y = src.position.top;
			dstPoint.y = dst.position.bottom;
		} else {
			srcPoint.y = src.position.bottom;
			dstPoint.y = dst.position.top;
		}
		return path([srcPoint, dstPoint]);
	}

	function renderConnectionPath(cpRef) {
		const path = line.interpolate('basis');
		const srcPoint = cpRef.position.centerPoint();
		const dstPoint = cpRef.parent.position.centerPoint();
		const srcIsTopMounted = /top/.test(cpRef.location);
		//const srcIsLeftMounted = /left/.test(cpRef.parent.location);
		const offset = 15;
		const srcSpline1 = {
			x: srcPoint.x,
			y: (srcIsTopMounted ? cpRef.position.top - offset : cpRef.position.bottom + offset)
		};
		const srcSpline2 = {
			x: srcPoint.x,
			y: (srcIsTopMounted ? cpRef.position.top - offset : cpRef.position.bottom + offset)
		};
		return path([srcPoint, srcSpline1, srcSpline2, dstPoint]);
	}

	const containerLayoutInfo = {
		nsd: {
			list: [],
			width: containerWidth,
			height: containerHeight,
			top() {
				return 10;
			},
			left(container, layouts) {
				const positionIndex = layouts.vnffgd.list.length + this.list.length;
				return (positionIndex * (this.width * 1.5)) + leftOffset / 2;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		vnffgd: {
			list: [],
			width: containerWidth,
			height: containerHeight,
			top(container, layouts) {
				const positionIndex = layouts.nsd.list.length + this.list.length;
				return (positionIndex * (this.height * 1.5)) + 120;
			},
			left(container, layouts) {
				return 10;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		vnfd: {
			list: [],
			width: containerWidth,
			height: containerHeight,
			top(container) {
				return (countAncestors(container) * 100) + 10;
			},
			left() {
				const positionIndex = this.list.length;
				return (positionIndex * (this.width * 1.5)) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		'constituent-vnfd': {
			list: [],
			width: containerWidth,
			height: containerHeight,
			top(container) {
				return (countAncestors(container) * 100) + 10;
			},
			left() {
				const positionIndex = this.list.length;
				return (positionIndex * (this.width * 1.5)) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		pnfd: {
			list: [],
			width: containerWidth,
			height: containerHeight,
			top(container) {
				return (countAncestors(container) * 100) + 10;
			},
			left() {
				const positionIndex = this.list.length;
				return (positionIndex * (this.width * 1.5)) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		vld: {
			list: [],
			width: containerWidth,
			height: 38,
			top(container) {
				return (countAncestors(container) * 100) + 180;
			},
			left() {
				const positionIndex = this.list.length;
				const marginOffsetFactor = 1.5;
				const gutterOffsetFactor = 1.5;
				return (positionIndex * (this.width * gutterOffsetFactor)) + ((this.width * marginOffsetFactor) / 2) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		'internal-vld': {
			list: [],
			width: containerWidth,
			height: 38,
			top(container, containerLayouts) {
				return (countAncestors(container) * 100) + 100;
			},
			left() {
				const positionIndex = this.list.length;
				const marginOffsetFactor = 1.5;
				const gutterOffsetFactor = 1.5;
				return (positionIndex * (this.width * gutterOffsetFactor)) + ((this.width * marginOffsetFactor) / 2) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		},
		vdu: {
			list: [],
			width: containerWidth,
			height: containerHeight,
			gutter: 30,
			top(container) {
				return (countAncestors(container) * 100) + 10;
			},
			left(container) {
				const positionIndex = this.list.length;
				return (positionIndex * (this.width * 1.5)) + leftOffset;
			},
			renderRelationPath: renderRelationPath,
			renderConnectionPath: renderConnectionPath
		}
	};

	function getConnectionPointEdges() {

		// 1. create a lookup map to find a connection-point by it's key
		const connectionPointMap = {};
		containerList.filter(d => d.connectionPoint).reduce((result, container) => {
			return container.connectionPoint.reduce((result, connectionPoint) => {
				result[connectionPoint.key] = connectionPoint;
				connectionPoint.uiState.hasConnection = false;
				return result;
			}, result);
		}, connectionPointMap);

		// 2. determine position of the connection-point and connection-point-ref (they are the same)
		const connectionPointRefList = [];
		containerList.filter(container => container.connection).forEach(container => {
			container.uiState.hasConnection = false;
			container.connection.filter(d => d.key).forEach(cpRef => {
				const source = connectionPointMap[cpRef.key];
				const destination = container;
				source.uiState.hasConnection = true;
				destination.uiState.hasConnection = true;
				const edgeStateMachine = math.upperLowerEdgeLocation;
				// angle is used to determine location top, bottom, right, left
				const angle = math.angleBetweenPositions(source.parent.position, destination.position);
				// distance is used to determine order of the connection points
				const distance = math.distanceBetweenPositions(source.parent.position, destination.position);
				cpRef.location = source.location = edgeStateMachine(angle);
				source.edgeAngle = angle;
				if (destination.type === 'vdu') {
					source.edgeLength = Math.max(source.edgeLength || 0, distance);
				}
				// warn assigning same instance (e.g. pass by reference) so that changes will reflect thru
				cpRef.position = source.position;
				connectionPointRefList.push(cpRef);
			});
		});

		// 3. update the connection-point/-ref location based on the angle of the path
		containerList.filter(d => d.connectionPoint).forEach(container => {
			// group the connectors by their location and then update their position coordinates accordingly
			const connectionPoints = container.connectionPoint.sort((a, b) => b.edgeLength - a.edgeLength);
			const locationIndexCounters = {};
			connectionPoints.forEach(connectionPoint => {
				// location index is used to calculate the actual position where the path will terminate on the container
				const location = connectionPoint.location;
				const locationIndex = locationIndexCounters[location] || (locationIndexCounters[location] = 0);
				connectionPoint.positionIndex = locationIndex;
				if (/left/.test(location)) {
					connectionPoint.position.moveLeft(connectionPoint.parent.position.left + 5 + ((connectionPoint.width + 1) * locationIndex));
				} else {
					connectionPoint.position.moveRight(connectionPoint.parent.position.right - 15 - ((connectionPoint.width + 1) * locationIndex));
				}
				if (/top/.test(location)) {
					connectionPoint.position.moveTop(connectionPoint.parent.position.top - connectionPoint.height);
				} else {
					connectionPoint.position.moveTop(connectionPoint.parent.position.bottom);
				}
				locationIndexCounters[location] = locationIndex + 1;
			});
		});

		return connectionPointRefList;

	}

	function drawConnectionPointsAndPaths(graph, connectionPointRefs) {

		const paths = graph.paths.selectAll('.connection').data(connectionPointRefs, DescriptorModelFactory.containerIdentity);

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
			d: edge => {
				const layout = containerLayoutInfo[edge.parent.type];
				return layout.renderConnectionPath(edge, containerLayoutInfo);
			}
		}).on('cut', (container) => {

			let success = false;

			if (container && container.remove) {
				success = container.remove();
			}

			if (success) {
				CatalogItemsActions.catalogItemDescriptorChanged.defer(container.getRoot());
			} else {
				d3.event.preventDefault();
			}

			d3.event.stopPropagation();

		});

		paths.exit().remove();

		const symbolSize = props.connectionPointSize;
		const connectionPointSymbol = d3.svg.symbol().type('square').size(symbolSize);
		const internalConnectionPointSymbolBottom = d3.svg.symbol().type('triangle-down').size(symbolSize);
		const internalConnectionPointSymbolTop = d3.svg.symbol().type('triangle-up').size(symbolSize);

		const connectors = containerList.filter(d => d.connectors).reduce((result, container) => {
			return container.connectors.reduce((result, connector) => {
				result.add(connector);
				return result;
			}, result);
		}, new Set());

		const points = graph.connectorsGroup.selectAll('.connector').data(Array.from(connectors), DescriptorModelFactory.containerIdentity);

		points.enter().append('path');

		points.attr({
			'data-uid': d => d.uid,
			'data-key': d => d.key,
			'data-cp-number': d => d.uiState.cpNumber,
			'class': d => {
				return ClassNames('connector', d.type, d.parent.type, {
					'-is-connected': d.uiState.hasConnection,
					'-is-not-connected': !d.uiState.hasConnection
				});
			},
			'data-tip': d => {
				const info = d.displayData;
				return Object.keys(info).reduce((r, key) => {
					if (info[key]) {
						return r + `<div class="${key}"><i>${key}</i><val>${info[key]}</val></div>`;
					}
					return r;
				}, '');
			},
			'data-tip-offset': d => {
				if (d.type === 'internal-connection-point') {
					return '{"top": -7, "left": -9}';
				}
				return '{"top": -5, "left": -5}';
			},
			transform: d => {
				const point = d.position.centerPoint();
				return 'translate(' + (point.x) + ', ' + (point.y) + ')';
			},
			d: d => {
				if (d.type === 'connection-point') {
					return connectionPointSymbol();
				}
				if (/top/.test(d.location)) {
					return internalConnectionPointSymbolTop();
				}
				return internalConnectionPointSymbolBottom();
			}
		}).on('cut', (container) => {

			let success = false;

			if (container && container.remove) {
				success = container.remove();
			}

			if (success) {
				CatalogItemsActions.catalogItemDescriptorChanged.defer(container.getRoot());
			} else {
				d3.event.preventDefault();
			}

			d3.event.stopPropagation();

		}).on('mouseenter', () => {
			TooltipManager.showTooltip(d3.event.target);
		});

		points.exit().remove();

		const test = new GraphConnectionPointNumber(graph);
		test.addContainers(Array.from(connectors));
		test.render();
	}

	function drawRelationPointsAndPaths (graph, relationEdges) {

		const paths = graph.paths.selectAll('.relation').data(relationEdges, DescriptorModelFactory.containerIdentity);

		paths.enter().append('path')
			.attr({
				'class': d => {
					return ClassNames('relation', d.type, {'-is-selected': d.uiState && SelectionManager.isSelected(d) /*d.uiState && d.uiState.selected*/});
				},
				stroke: 'red',
				fill: 'transparent',
				'marker-start': 'url(#relation-marker-end)',
				'marker-end': 'url(#relation-marker-end)'
			});

		paths.attr({
			d: d => {
				const src = d;
				const dst = d.parent;
				const layout = containerLayoutInfo[src.type];
				return layout.renderRelationPath(src, dst, containerLayoutInfo);
			}
		});

		paths.exit().remove();

	}

	function updateContainerPosition(graph, container, layout) {
		// use the provided layout to generate the position coordinates
		const position = container.position;
		position.top = layout.top(container, containerLayoutInfo) + marginTop;
		position.left = layout.left(container, containerLayoutInfo) + marginLeft;
		position.width = layout.width;
		position.height = layout.height;
		// cache the default layout position which may be needed by the layout
		// of children elements that have not been positioned by the user
		container.uiState.defaultLayoutPosition = position.value();
		const savedContainerPosition = graph.lookupSavedContainerPosition(container);
		if (savedContainerPosition) {
			// set the container position with the saved position coordinates
			container.setPosition(savedContainerPosition);
		}
		if (container.uiState.dropCoordinates) {
			const rect = graph.svg.node().getBoundingClientRect();
			const top = container.uiState.dropCoordinates.clientY - (position.height / 2) - rect.top;
			const left = container.uiState.dropCoordinates.clientX - (position.width / 2) - rect.left;
			container.position.move(Math.max(props.padding, left), Math.max(props.padding, top));
			graph.saveContainerPosition(container);
			delete container.uiState.dropCoordinates;
		} else {
			graph.saveContainerPosition(container);
		}
	}

	return {

		addContainers(containers) {

			const layout = this;

			//containers = containers.filter(d => containerLayouts[d.type]);

			const graphSize = {
				width: 0,
				height: 0
			};

			containers.forEach(container => {
				containerList.push(container);
			});

			containers.forEach(container => {
				const layout = containerLayoutInfo[container.type];
				if (!layout) {
					return
					//throw new ReferenceError('unknown container type: ' + container.type);
				}
				updateContainerPosition(graph, container, layout);
				layout.list.push(container);
				graphSize.width = Math.max(graphSize.width, container.position.right, props.width);
				graphSize.height = Math.max(graphSize.height, container.position.bottom, props.height);
			});

			graph.svg.attr({
				width: graphSize.width + props.width,
				height: graphSize.height + props.height
			});

			const uiTransientState = {
				isDragging: false,
				dragStartInfo: [0, 0]
			};

			// todo extract drag behavior into class DescriptorGraphDrag

			const drag = this.drag = d3.behavior.drag()
				.origin(function(d) { return d; })
				.on('drag.graph', function (d) {
					uiTransientState.isDragging = true;
					const mouse = d3.mouse(graph.g.node());
					const offset = uiTransientState.dragStartInfo;
					const newTopEdge = snapTo(mouse[1] - offset[1]);
					const newLeftEdge = snapTo(mouse[0] - offset[0]);
					if (d.position.left === newLeftEdge && d.position.top === newTopEdge) {
						// do not redraw since we are not moving the container
						return;
					}
					d.position.move(newLeftEdge, newTopEdge);
					graph.saveContainerPosition(d);
					const connectionPointRefs = getConnectionPointEdges();
					d3.select(this).attr({
						transform: () => {
							const x = d.position.left;
							const y = d.position.top;
							return 'translate(' + x + ', ' + y + ')';
						}
					});
					requestAnimationFrame(() => {
						drawConnectionPointsAndPaths(graph, connectionPointRefs);
						layout.renderers.forEach(d => d.render());
					});
				}).on('dragend.graph', () => {
					// d3 fires a drag-end event on mouse up, even if just clicking
					if (uiTransientState.isDragging) {
						uiTransientState.isDragging = false;
						CatalogItemsActions.catalogItemMetaDataChanged(graph.containers[0].model);
						d3.select(this).on('.graph', null);
					}
				}).on('dragstart.graph', function (d) {
					// the x, y offset of the mouse from the container's left, top
					uiTransientState.dragStartInfo = d3.mouse(this);
				});

			this.renderers = [GraphVirtualLink, GraphNetworkService, GraphForwardingGraph, GraphVirtualNetworkFunction, GraphConstituentVnfd, GraphVirtualDeploymentUnit, GraphRecordServicePath, GraphInternalVirtualLink].map(layout => {
				const container = new layout(graph, props);
				const layoutInfo = containerLayoutInfo[container.classType && container.classType.type];
				if (layoutInfo) {
					container.props.descriptorWidth = layoutInfo.width;
					container.props.descriptorHeight = layoutInfo.height;
				}
				container.dragHandler = drag;
				container.addContainers(containerList);
				return container;
			});

		},

		render(graph, updateCallback = () => {}) {
			const connectionPointRefs = getConnectionPointEdges();
			requestAnimationFrame(() => {
				drawConnectionPointsAndPaths(graph, connectionPointRefs);
				this.renderers.forEach(d => d.render());
				updateCallback();
			});
		}

	};

}