
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/2/15.
 */

import _ from 'lodash'
import d3 from 'd3'
import UID from './UniqueId'
import SelectionManager from './SelectionManager'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import DescriptorModelFactory from './model/DescriptorModelFactory'

import '../styles/Animations.scss'

const DELETE = 26;
const BACKSPACE = 8;

function onlyUnique(value, index, self) {
	return self.indexOf(value) === index;
}

function createDeleteEvent(e, uid, eventName = 'cut') {
	const data = {
		bubbles: true,
		cancelable: true,
		detail: {
			uid: uid,
			originalEvent: e
		}
	};
	if (window.CustomEvent.prototype.initCustomEvent) {
		// support IE
		var evt = document.createEvent('CustomEvent');
		evt.initCustomEvent(eventName, data.bubbles, data.cancelable, data.detail);
		return evt;
	}
	return new CustomEvent(eventName, data);
}

export default class DeletionManager {

	static onDeleteKey(event) {
		const target = event.target;
		if (event.defaultPrevented) {
			return
		}
		if ((event.which === DELETE || event.which === BACKSPACE) && /^BODY|SVG|DIV$/i.test(target.tagName)) {
			event.preventDefault();
			DeletionManager.deleteSelected(event);
			return false;
		}
	}

	static deleteSelected(event) {

		// TODO refactor this to be a flux action e.g. move this code into ComposerAppActions.deleteSelected()

		const selected = SelectionManager.getSelections();

		const canvasPanelDiv = document.getElementById('canvasPanelDiv');

		if (!canvasPanelDiv) {
			return
		}

		// get a valid list of items to potentially remove via the cut event handler
		const removeElementList = selected.filter(d => d).filter(onlyUnique).reduce((r, uid) => {
			const elements = Array.from(canvasPanelDiv.querySelectorAll('[data-uid="' + uid + '"]'));
			return r.concat(elements);
		}, []).filter(d => d);

		if (removeElementList.length === 0 && selected.length > 0) {
			// something was selected but we did not find any dom elements with data-uid!
			console.error(`No valid DescriptorModel instance found on element. Did you forget to put data-uid={m.uid}`,
				selected.map(uid => Array.from(canvasPanelDiv.querySelectorAll(`[data-uid="${uid}"]`))));
		}

		SelectionManager.removeOutline();

		// now actually update the model
		const invokedEventAlreadyMap = {};
		const failedToRemoveList = removeElementList.map(element => {

			const uid = UID.from(element);

			if (invokedEventAlreadyMap[uid]) {
				return
			}

			try {
				// https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/dispatchEvent
				// false means event.preventDefault() was called by one of the handlers
				const deleteEvent = createDeleteEvent(event, uid);
				const preventDefault = (false === element.dispatchEvent(deleteEvent));
				if (preventDefault) {
					console.log('cut event was cancelled', element);
					//d3.select(element).classed('-with-animations deleteItemAnimation', false).style({opacity: null});
					return element;
				}

			} catch (error) {
				console.warn(`Exception caught dispatching 'cut' event: ${error}`,
					selected.map(uid => Array.from(canvasPanelDiv.querySelectorAll(`[data-uid="${uid}"]`)))
				);
				return element;
			} finally {
				invokedEventAlreadyMap[uid] = true;
			}

		}).filter(d => d).filter(onlyUnique);

		SelectionManager.clearSelectionAndRemoveOutline();
		failedToRemoveList.forEach(d => SelectionManager.addSelection(d));
		SelectionManager.refreshOutline();

	}

	static addEventListeners() {
		DeletionManager.removeEventListeners();
		document.body.addEventListener('keydown', DeletionManager.onDeleteKey);
	}

	static removeEventListeners() {
		document.body.removeEventListener('keydown', DeletionManager.onDeleteKey);
	}

}
