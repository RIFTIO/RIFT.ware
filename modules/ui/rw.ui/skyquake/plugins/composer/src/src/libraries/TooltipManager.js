/*global Element*/
/**
 * Created by onvelocity on 2/22/16.
 */

'use strict';

import d3 from 'd3'

import '../styles/TooltipManager.scss'

class TooltipManager {

	static addEventListeners(element = document.body) {
		TooltipManager.element = element;
		TooltipManager.removeEventListeners();
		TooltipManager.element.addEventListener('mousedown', TooltipManager.onScrollRemoveTooltip, true);
		TooltipManager.element.addEventListener('scroll', TooltipManager.onScrollRemoveTooltip, true);
	}

	static removeEventListeners() {
		if (TooltipManager.element) {
			TooltipManager.element.removeEventListener('mousedown', TooltipManager.onScrollRemoveTooltip, true);
			TooltipManager.element.removeEventListener('scroll', TooltipManager.onScrollRemoveTooltip, true);
		}
	}

	static onScrollRemoveTooltip() {
		TooltipManager.hideTooltip();
	}

	static showTooltip(dom, timeout = 2500) {
		TooltipManager.hideTooltip();
		const data = d3.select(dom).attr('data-tip');
		const tip = d3.select(TooltipManager.element).selectAll('.tooltip-indicator').data([data]);
		const rect = dom.getBoundingClientRect();
		// position is handled fairly well by CSS for the bottom (default) position
		// new code will be needed to support other positions, e.g. top, left, etc.
		const position = {
			opacity: 0,
			position: 'absolute',
			'pointer-events': 'none',
			top: rect.bottom + 'px' ,
			left: (rect.left + (rect.width / 2)) + 'px'
		};
		const enter = tip.enter('div').append('div').classed('tooltip-indicator', true);
		enter.append('i').classed('tooltip-indicator-arrow arrow-up', true);
		enter.append('span').classed('tooltip-indicator-content', true);
		tip.style(position).transition().style('opacity', 1);
		tip.select('span').html(d => d);
		if (timeout > 0) {
			TooltipManager.timeoutId = setTimeout(() => tip.transition().style('opacity', 0), timeout);
		}
		tip.exit().remove();
	}

	static hideTooltip() {
		clearTimeout(TooltipManager.timeoutId);
		d3.selectAll('.tooltip-indicator').remove();
	}

}

TooltipManager.addEventListeners();

export default TooltipManager;
