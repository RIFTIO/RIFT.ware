/**
 * Created by onvelocity on 3/2/16.
 */
'use strict';
import utils from '../../libraries/utils'
import DescriptorModelFactory from '../../libraries/model/DescriptorModelFactory'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
export default function onFormInputChangedModifyContainerAndNotify(container, event) {
	if (DescriptorModelFactory.isContainer(container)) {
		event.preventDefault();
		const name = event.target.name;
		const value = event.target.value;
		utils.assignPathValue(container.model, name, value);
		CatalogItemsActions.catalogItemDescriptorChanged(container.getRoot());
	} else {
		throw new TypeError('onFormInputChangedModifyContainerAndNotify called without a DescriptorModel. Did you forget to bind a DescriptorModel to the event handler?');
	}
}
