/**
 * Created by onvelocity on 3/4/16.
 */
'use strict';
import d3 from 'd3'
import TooltipManager from '../../libraries/TooltipManager'
export default function onHoverHighlightConnectionPoint(cpNumber, event) {
	const found = Array.from(document.querySelectorAll('[data-cp-number="' + cpNumber + '"]'));
	if (event.type === 'mouseenter') {
		found.forEach(d => {
			d3.select(d).classed('-is-highlight', true);
			if (d3.select(d).attr('data-tip')) {
				TooltipManager.showTooltip(d, 0);
			}
		});
	} else {
		TooltipManager.hideTooltip();
		found.forEach(d => d3.select(d).classed('-is-highlight', false));
	}
}
