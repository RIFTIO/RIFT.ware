
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/14/15.
 */
import alt from '../alt';

class CanvasEditorActions {

	constructor() {
		this.generateActions('showMoreInfo', 'showLessInfo', 'toggleShowMoreInfo', 'applyDefaultLayout', 'setCanvasZoom', 'addVirtualLinkDescriptor', 'addForwardingGraphDescriptor', 'addVirtualDeploymentDescriptor');
	}

}

export default alt.createActions(CanvasEditorActions);
