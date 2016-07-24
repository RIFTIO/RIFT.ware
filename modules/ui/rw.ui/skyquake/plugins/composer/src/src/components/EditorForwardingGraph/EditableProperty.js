/**
 * Created by onvelocity on 3/3/16.
 */
'use strict';
import React from 'react'
import ClassNames from 'classnames'
import '../../styles/EditableProperty.scss'
export default function EditableProperty(props) {
	return (
		<div className={ClassNames('EditableProperty', 'property', {'-is-disabled': props.disabled})}>
			<h3 className="property-label">{props.title}</h3>
			<div className="property-description">{props.description}</div>
			<val className="property-value"><div className="property-content">{props.children}</div></val>
		</div>
	)
}
