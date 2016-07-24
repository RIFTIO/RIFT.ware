/**
 * Created by onvelocity on 2/2/16.
 */

import React from 'react'
import OpenWindow from 'react-popout'
import JSONViewer from './JSONViewer'
import ComposerAppActions from '../actions/ComposerAppActions'

import '../styles/JSONViewer.scss'

window._open = window.open.bind(window);
window.open = function monkeyPatchOpen(url, name, options) {
	const popupWindow = this._open(url, name, options);
	popupWindow.document.getElementsByTagName('body')[0].focus();
	return popupWindow;
};

export default function (props) {
	if (!props.show) {
		return <div></div>;
	}
	function onClose() {
		ComposerAppActions.closeJsonViewer();
	}
	return (
		<OpenWindow title={"RIFT.ware Popup"} onClosing={onClose}>
			<div>
				<h1>{props.title}</h1>
				{props.children}
			</div>
		</OpenWindow>
	);
}
