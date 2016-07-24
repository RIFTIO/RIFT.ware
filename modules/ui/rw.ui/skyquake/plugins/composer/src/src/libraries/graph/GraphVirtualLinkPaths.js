/**
 * Created by onvelocity on 2/15/16.
 */
import ColorGroups from '../ColorGroups'

/**
 * NOTE: WIP this will replace part of the RelationsAndNetworksLayout::drawConnectionPointsAndPaths method.
 *
 * This class draws the paths between the VLD and the Connection Point it references.
 *
 */
export default class GraphVirtualLinkPaths {

	constructor(graph, props) {
		this.graph = graph;
		this.props = props;
		this.containers = [];
	}

	addContainers(containers) {

	}

	render() {

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
				const layout = containerLayouts[edge.parent.type];
				return layout.renderConnectionPath(edge, containerLayouts);
			}
		});

		paths.exit().remove();

	}

}