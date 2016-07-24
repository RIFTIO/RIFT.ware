/**
 * Created by onvelocity on 2/4/16.
 */
'use strict';
import React from 'react'
import ClassNames from 'classnames'
import ResizableManager from '../libraries/ResizableManager'
import CanvasPanelTrayActions from '../actions/CanvasPanelTrayActions'
import '../styles/CanvasPanelTray.scss'
const uiTransient = {
	isResizing: false
};
export default function (props) {
	const style = {
		height: Math.max(0, props.layout.bottom),
		right: props.layout.right,
		display: props.show ? false : 'none'
	};
	const classNames = ClassNames('CanvasPanelTray', {'-with-transitions': !document.body.classList.contains('resizing')});
	function onClickToggleOpenClose(event) {
		if (event.defaultPrevented) return;
		event.preventDefault();
		// don't toggle if the user was resizing
		if (!uiTransient.isResizing) {
			CanvasPanelTrayActions.toggleOpenClose();
		}
		event.target.removeEventListener('mousemove', onMouseMove, true);
	}
	function onMouseDown(event) {
		uiTransient.isResizing = false;
		event.target.addEventListener('mousemove', onMouseMove, true);
	}
	function onMouseMove() {
		uiTransient.isResizing = ResizableManager.isResizing();
	}
	// note 25px is the height of the h1
	const isOpen = style.height > 25;
	return (
		<div className={classNames} data-resizable="top" data-resizable-handle-offset="4" style={style}>
			<h1 data-open-close-icon={isOpen ? 'open' : 'closed'} onMouseDownCapture={onMouseDown} onClick={onClickToggleOpenClose}>Forwarding Graphs</h1>
			<div className="tray-body">
				{props.children}
			</div>
		</div>
	);
}