/**
 * Created by onvelocity on 2/13/16.
 */
'use strict';
import React from 'react'
import ClassNames from 'classnames'
export default function ContentEditableDiv (props) {

	const fontWidth = parseFloat(props.fontWidth) || 15;
	const size = props.autoPadRight ? Math.max(50, String(props.name).length * fontWidth) : 0;
	const style = {borderColor: 'transparent', background: 'transparent'};
	if (size) {
		style.paddingRight = size;
	}

	return (
		<div className={ClassNames('ContentEditableDiv', {'-is-disabled': props.disabled})}><input {...props} style={style} /></div>
	);

}