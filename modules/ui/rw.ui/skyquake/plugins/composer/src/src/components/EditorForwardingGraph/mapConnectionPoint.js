/**
 * Created by onvelocity on 3/4/16.
 */
'use strict'
import React from 'react'
import onCutDelegateToRemove from './onCutDelegateToRemove'
import onHoverHighlightConnectionPoint from './onHoverHighlightConnectionPoint'
import onClickSelectAndShowInDetailsPanel from './onClickSelectAndShowInDetailsPanel'
export default function mapConnectionPoint(style, addLine, cpRef, i) {
	return (
		<div key={cpRef.uid || i} className="rsp">
			{addLine ? <div className="rsp-line"></div> : null}
			<div className={cpRef.className} data-uid={cpRef.uid} style={style}
				 onCut={onCutDelegateToRemove.bind(null, cpRef)}
				 onMouseEnter={onHoverHighlightConnectionPoint.bind(null, cpRef.cpNumber)}
				 onMouseLeave={onHoverHighlightConnectionPoint.bind(null, cpRef.cpNumber)}
				 onClick={onClickSelectAndShowInDetailsPanel.bind(null, cpRef)}>
				<small>{cpRef.cpNumber || cpRef.vnfdConnectionPointName}</small>
			</div>
		</div>
	);
}
