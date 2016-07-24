/**
 * Created by onvelocity on 2/3/16.
 */
'use strict';
const pairedColors = ['#fb9a99','#e31a1c','#a6cee3','#1f78b4','#b2df8a','#33a02c','#fdbf6f','#ff7f00','#cab2d6','#6a3d9a','#ffff99','#b15928'];

import colorTheme from '../assets/rift.ware-color-theme.json'

/**
 *
 * Colors are also defined in _ColorGroups.scss. HINT: Uncomment the console statement at the bottom of this file to
 * generate the variables and then copy and paste them into the .scss file.
 *
 * Externalized colors into a .json file to make it easier to modify them.
 */
const ColorGroups = {
	// http://colorbrewer2.org/?type=qualitative&scheme=Paired&n=12
	getColorPair(index) {
		// make sure index is not larger than our list of colors
		index = (index * 2) % 12;
		return {
			primary: pairedColors[index + 1],
			secondary: pairedColors[index]
		};
	},
	getColorCSS() {
		return Object.keys(colorTheme).map((key) => {
			if (typeof colorTheme[key] === 'object') {
				const color = colorTheme[key];
				return Object.keys(color).map(name => {
					return `\$${key}-${name}-color: ${color[name]};`
				}).join('\n');
			}
			if (typeof colorTheme[key] === 'string') {
				const color = colorTheme[key];
				return `$descriptor-${key}: ${color};\n\n`
			}
		}).join('\n\n');
	},
	getColorPairForType(type) {
		const colors = colorTheme[type];
		if (colors) {
			return colors;
		}
		return colorTheme.common;
	}
};

//console.log(ColorGroups.getColorCSS());

Object.assign(ColorGroups, colorTheme);

export default ColorGroups;
