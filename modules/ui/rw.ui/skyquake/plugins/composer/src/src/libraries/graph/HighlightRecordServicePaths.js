/**
 * Created by onvelocity on 2/23/16.
 */

'use strict';

import _ from 'lodash'
import d3 from 'd3'

/**
 * Add CSS classes to Rendered Service Paths (Record Service Path in code) so they hightlight.
 */
export default class HighlightRecordServicePaths {

	static removeHighlighting() {
		Array.from(document.querySelectorAll(`svg .forwarding-graph-paths`)).forEach(d => {
			d3.select(d).classed('-is-highlighting', false);
		});
		Array.from(document.querySelectorAll('.rsp-path')).forEach(d => {
			d3.select(d).classed('-is-highlighted', false);
		});
	}

	static highlightPaths(rsp) {
		HighlightRecordServicePaths.removeHighlighting();
		Array.from(document.querySelectorAll(`svg .forwarding-graph-paths`)).forEach(d => {
			d3.select(d).classed('-is-highlighting', true);
		});
		const list = _.isArray(rsp) ? rsp : [rsp];
		list.forEach(rsp => {
			Array.from(document.querySelectorAll(`[data-id="${rsp.id}"]`)).forEach(d => {
				d.parentNode.appendChild(d);
				d3.select(d).classed('-is-highlighted', true);
			});
		});
	}

}
