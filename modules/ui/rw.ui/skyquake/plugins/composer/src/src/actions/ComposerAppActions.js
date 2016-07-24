
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by kkashalk on 11/30/15.
 */
import alt from '../alt';

class ComposerAppActions {

	constructor() {
		this.generateActions('showError', 'clearError', 'setDragState', 'propertySelected', 'showJsonViewer', 'closeJsonViewer', 'selectModel', 'outlineModel', 'clearSelection', 'enterFullScreenMode', 'exitFullScreenMode');
	}

}

export default alt.createActions(ComposerAppActions);