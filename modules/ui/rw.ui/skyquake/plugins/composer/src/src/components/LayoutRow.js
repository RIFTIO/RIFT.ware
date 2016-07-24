/**
 * Created by onvelocity on 2/12/16.
 */

'use strict';

import React from 'react'
import ClassNames from 'classnames'

import '../styles/LayoutRow.scss'

export default function LayoutRow(props) {

	const primaryActionColumn = props.primaryActionColumn ? props.primaryActionColumn : <div className="layout-row-empty-cell"> </div>;
	const secondaryActionColumn = props.secondaryActionColumn ? props.secondaryActionColumn : <div className="layout-row-empty-cell"> </div>;

	return (
		<table {...props} className={ClassNames('layout-row', props.className)}>
			<tbody>
				<tr>
					<th className="primary-action-column">
						<div className="primary-action-column">{primaryActionColumn}</div>
					</th>
					<th className="secondary-action-column">
						<div className="primary-action-column">{secondaryActionColumn}</div>
					</th>
					<td className="columns">
						{props.children}
					</td>
				</tr>
			</tbody>
		</table>
	);

}
