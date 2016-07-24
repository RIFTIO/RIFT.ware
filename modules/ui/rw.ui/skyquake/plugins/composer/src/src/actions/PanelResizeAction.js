
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 8/11/15.
 */
import alt from '../alt';
import changeCase from 'change-case'

/*
 This class manages Composer Layout State
 */

const cleanNameRegExp = /(-is-tray-open|panel-)/i;

class PanelResizeAction {

	resize(e) {

		/* we expect two types of resize events:
		 * window resize - invoked by window
		 * resize-manager resize - invoked by ResizeManager
		 *
		 * normalize the data needed by the Composer Layout or ignore invalid ones
		 *
		 * */

		if (!e) {
			return false;
		}

		if (e.detail && e.detail.side) {
			// a ResizeManager event
			this.dispatch(PanelResizeAction.buildResizeManagerInfo(e))
		} else {
			// a window event
			this.dispatch(PanelResizeAction.buildWindowResizeInfo(e));
		}

	}

	static buildWindowResizeInfo(e) {
		return e;
	}

	static buildResizeManagerInfo(e) {
		const info = Object.assign({originalEvent: e}, e.detail);
		const name = changeCase.paramCase(info.target.className.replace(cleanNameRegExp, ''));
		info.type = 'resize-manager.resize.' + name;
		return info;
	}

}

export default alt.createActions(PanelResizeAction);
