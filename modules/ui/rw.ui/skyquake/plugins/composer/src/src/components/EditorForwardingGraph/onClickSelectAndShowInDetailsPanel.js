/**
 * Created by onvelocity on 3/2/16.
 */
'use strict';
import ComposerAppActions from '../../actions/ComposerAppActions'
export default function onClickSelectAndShowInDetailsPanel(container, event) {
	if (event.defaultPrevented) return;
	event.preventDefault();
	if (container.isFactory) {
		return
	}
	ComposerAppActions.selectModel(container);
	ComposerAppActions.outlineModel.defer(container);
}
