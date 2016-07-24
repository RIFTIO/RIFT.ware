/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Draw edges by dragging a VirtualNetworkFunctionConnectionPoint to VNFD, VLD or another VirtualNetworkFunctionConnectionPoint.
 *
 * If VLD does not exist between two VNFDs then add one.
 *
 * CSS for this class is defined in DescriptorGraph.scss.
 *
 */

import d3 from 'd3'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import SelectionManager from '../SelectionManager'
import DescriptorModelFactory from '../model/DescriptorModelFactory'

const line = d3.svg.line()
	.x(d => {
		return d.x;
	})
	.y(d => {
		return d.y;
	});

function mouseWithinPosition(position, mouseCoordinates, scale = 1) {
	const x = mouseCoordinates[0] / scale;
	const y = mouseCoordinates[1] / scale;
	const withinXBoundary = position.left < x && position.right > x;
	const withinYBoundary = position.top < y && position.bottom > y;
	return withinXBoundary && withinYBoundary;
}

function getContainerUnderMouse(comp, element, scale) {
	const mouseCoordinates = d3.mouse(element);
	return comp.descriptorsAndConnectionPoints().filter(d => {
		return DescriptorModelFactory.isConnectionPoint(d) ||
			DescriptorModelFactory.isInternalConnectionPoint(d) ||
			DescriptorModelFactory.isVirtualLink(d) ||
			DescriptorModelFactory.isInternalVirtualLink(d);
	}).filter(d => mouseWithinPosition(d.position, mouseCoordinates, scale));
}

export default class DescriptorGraphPathBuilder {

	constructor(graph) {
		this.graph = graph;
		this.containers = [];
	}

	descriptorsAndConnectionPoints() {
		return this.graph.g.selectAll('.descriptor, .connector');
	}

	descriptors() {
		return this.graph.containersGroup.selectAll('.descriptor');
	}

	connectionPoints() {
		return this.graph.connectorsGroup.selectAll('.connector');
	}

	paths() {
		return this.graph.g.selectAll('.connection');
	}

	addContainers(containers) {
		this.containers = this.containers.concat(containers);
	}

	static addConnection(srcConnector, dstConnector) {

		// return true on success; falsy otherwise to allow the caller to clean up accordingly

		if (!(srcConnector || dstConnector)) {
			return false;
		}

		if (srcConnector.canConnectTo && srcConnector.canConnectTo(dstConnector)) {

			const root = srcConnector.getRoot();

			if (DescriptorModelFactory.isVirtualLink(dstConnector) || DescriptorModelFactory.isInternalVirtualLink(dstConnector)) {
				dstConnector.addConnectionPoint(srcConnector);
			} else {
				if (root) {
					const vld = root.createVld();
					root.removeAnyConnectionsForConnector(srcConnector);
					root.removeAnyConnectionsForConnector(dstConnector);
					vld.addConnectionPoint(srcConnector);
					vld.addConnectionPoint(dstConnector);
				}
			}

			// notify catalog items have changed to force a redraw and update state accordingly
			CatalogItemsActions.catalogItemDescriptorChanged(root);

			return true;

		}

	}

	static addConnectionToVLD(srcSelection, dstSelection) {

		if (srcSelection[0].length === 0 || dstSelection[0].length === 0) {
			return false;
		}

		const dstVirtualLink = dstSelection.datum();
		const srcConnectionPoint = srcSelection.datum();

		dstVirtualLink.addConnectionPoint(srcConnectionPoint);

		// notify catalog items have changed to force a redraw and update state accordingly
		CatalogItemsActions.catalogItemDescriptorChanged(dstVirtualLink.getRoot());

		return true;

	}

	render() {

		/*
			Strategy: compare mouse position with the position of all the selectable elements.
			On mouse down:
			    determine if there is a connection point under the mouse (the source)
			    determine if there is already a path connected on this source
			On mouse move:
			    draw a tracer line from the source to the mouse
			On mouse up:
			    determine if there is a connection point or VLD under the mouse (the destination)
			    take the respective action and notify
		 */

		const comp = this;
		const drawLine = line.interpolate('basis');

		comp.boundMouseDownHandler = function (d) {

			let hasInitializedMouseMoveHandler = false;

			const srcConnectionPoint = comp.connectionPoints().filter(d => {
				return DescriptorModelFactory.isConnectionPoint(d) || DescriptorModelFactory.isInternalConnectionPoint(d);
			}).filter(d => mouseWithinPosition(d.position, d3.mouse(this), comp.graph.scale));

			if (srcConnectionPoint[0].length) {

				const src = srcConnectionPoint.datum() || {};

				// determine if there is already a path on this connection point
				// if there is then hide it; the mouseup handler will clean up
				const existingPath = comp.paths().filter(d => {
					return d && d.parent && d.parent.type === 'vld' && d.key === src.key;
				});

				// create a new path to follow the mouse
				const path = comp.graph.paths.selectAll('.new-connection').data([srcConnectionPoint.datum()], DescriptorModelFactory.containerIdentity);
				path.enter().append('path').classed('new-connection', true);

				comp.boundMouseMoveHandler = function () {

					const mouseCoordinates = d3.mouse(this);

					path.attr({
						fill: 'red',
						stroke: 'red',
						'stroke-width': '4px',
						d: d => {
							const srcPosition = d.position.centerPoint();
							const dstPosition = {
								x: mouseCoordinates[0] / comp.graph.scale,
								y: mouseCoordinates[1] / comp.graph.scale
							};
							return drawLine([srcPosition, dstPosition]);
						}
					});

					if (!hasInitializedMouseMoveHandler) {

						hasInitializedMouseMoveHandler = true;

						SelectionManager.removeOutline();

						existingPath.style({
							opacity: 0
						});

						// allow for visual treatment of this drag operation
						srcConnectionPoint.classed('-selected', true);

						// allow for visual treatment of this drag operation
						comp.graph.svg.classed('-is-dragging-connection-point', true);

						// identify which descriptors are a valid drop target
						comp.descriptors().filter(d => {
							const validTarget = src.canConnectTo && src.canConnectTo(d);
							return validTarget;
						}).classed('-is-valid-drop-target', true);

						// identify which connection points are a valid drop target
						comp.connectionPoints().filter(d => {
							const validTarget = src.canConnectTo && src.canConnectTo(d);
							return validTarget;
						}).classed('-is-valid-drop-target', true);

					}

					const validDropTarget = getContainerUnderMouse(comp, this, comp.graph.scale);
					comp.graph.g.selectAll('.-is-drag-over').classed('-is-drag-over', false);
					SelectionManager.removeOutline();
					if (validDropTarget) {
						validDropTarget
							.filter(d => src.canConnectTo && src.canConnectTo(d))
							.classed('-is-drag-over', true)
							.each(function () {
								// warn must be a function so 'this' will = the path element
								SelectionManager.outlineSvg(this);
							});
					}

				};

				// determine what the interaction is and do it
				comp.boundMouseUpHandler = function () {

					// remove these handlers so they start fresh on the next drag operation
					comp.graph.svg
						.on('mouseup.edgeBuilder', null)
						.on('mousemove.edgeBuilder', null);

					// remove visual treatments
					comp.graph.svg.classed('-is-dragging-connection-point', false);
					comp.descriptors().classed('-is-valid-drop-target', false);
					comp.connectionPoints().classed('-is-valid-drop-target', false);
					comp.graph.g.selectAll('.-is-drag-over').classed('-is-drag-over', false);

					const dstSelection = getContainerUnderMouse(comp, this, comp.graph.scale);

					// determine if there is a connection point
					if (dstSelection[0].length) {
						if (DescriptorGraphPathBuilder.addConnection(src, dstSelection.datum())) {
							existingPath.remove();
						}
					}

					// if we hid an existing path restore it
					existingPath.style({
						opacity: null
					});

					// remove the tracer path
					path.remove();

					SelectionManager.refreshOutline();

				};

				// init drag handlers
				comp.graph.svg
					.on('mouseup.edgeBuilder', comp.boundMouseUpHandler)
					.on('mousemove.edgeBuilder', comp.boundMouseMoveHandler);
			}

		};

		// enable dragging features
		comp.graph.svg
			.on('mousedown.edgeBuilder', comp.boundMouseDownHandler);

	}

}
