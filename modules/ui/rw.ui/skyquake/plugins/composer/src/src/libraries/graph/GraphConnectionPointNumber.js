/**
 * Created by onvelocity on 2/8/16.
 */
'use strict';
import DescriptorModelFactory from '../model/DescriptorModelFactory'
const defaults = {
	adjustTop: -9,
	adjustBottom: 21
};
export default class GraphConnectionPointNumber {

	constructor(graph, props) {
		this.cpNumbersGroup = graph.cpNumbers;
		Object.assign(this, defaults, props);
		this.containers = [];
	}

	addContainers(containers) {
		this.containers = containers.filter(d => DescriptorModelFactory.isConnectionPoint(d) && DescriptorModelFactory.isNetworkService(d.getRoot()));
	}

	render() {
		const cpNumber = this.cpNumbersGroup.selectAll('.connection-point-number').data(this.containers, d => d.uid);
		cpNumber.enter().append('text').text(d => d.uiState.cpNumber);
		cpNumber.attr({
			'data-key': d => d.key,
			'data-cp-number': d => d.uiState.cpNumber,
			'text-anchor': 'middle',
			'class': 'connection-point-number',
			transform: d => {
				const point = d.position.centerPoint();
				const yAdjust = (/top/.test(d.location) ? this.adjustTop : this.adjustBottom);
				return 'translate(' + (point.x) + ', ' + (point.y + yAdjust) + ')';
			}
		});
		cpNumber.exit().remove();
	}

}
