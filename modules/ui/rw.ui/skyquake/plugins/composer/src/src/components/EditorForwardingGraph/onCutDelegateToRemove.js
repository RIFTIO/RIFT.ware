/**
 * Created by onvelocity on 3/2/16.
 */
'use strict';
import CatalogItemsActions from '../../actions/CatalogItemsActions'
export default function onCutDelegateToRemove(container, event) {
	if (event.defaultPrevented) return;
	if (container.remove()) {
		CatalogItemsActions.catalogItemDescriptorChanged.defer(container.getRoot());
	} else {
		event.preventDefault();
	}
	event.stopPropagation();
}
