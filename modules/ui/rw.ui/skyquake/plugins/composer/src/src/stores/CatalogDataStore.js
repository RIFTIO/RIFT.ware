
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import cc from 'change-case'
import alt from '../alt'
import UID from '../libraries/UniqueId'
import guid from '../libraries/guid'
import React from 'react'
import DescriptorModel from '../libraries/model/DescriptorModel'
import DescriptorModelMetaFactory from '../libraries/model/DescriptorModelMetaFactory'
import CatalogPackageManagerActions from '../actions/CatalogPackageManagerActions'
import CatalogDataSourceActions from '../actions/CatalogDataSourceActions'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import ModalOverlayActions from '../actions/ModalOverlayActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import CatalogDataSource from '../sources/CatalogDataSource'
import ComposerAppStore from '../stores/ComposerAppStore'
import SelectionManager from '../libraries/SelectionManager'

const defaults = {
	catalogs: [],
	catalogItemExportFormats: ['rift', 'mano']
};

const areCatalogItemsMetaDataEqual = function (a, b) {
	const metaProps = ['id', 'name', 'short-name', 'description', 'vendor', 'version'];
	const aMetaData = _.pick(a, metaProps);
	const bMetaData = _.pick(b, metaProps);
	return _.isEqual(aMetaData, bMetaData);
};

class CatalogDataStore {

	constructor() {
		this.catalogs = defaults.catalogs;
		this.isLoading = true;
		this.requiresSave = false;
		this.snapshots = {};
		this.registerAsync(CatalogDataSource);
		this.bindActions(CatalogDataSourceActions);
		this.bindActions(CatalogItemsActions);
	}

	getCatalogs() {
		return this.catalogs || (this.catalogs = []);
	}

	getAllSelectedCatalogItems() {
		return this.getCatalogs().reduce((r, d) => {
			d.descriptors.forEach(d => {
				if (SelectionManager.isSelected(d) /*d.uiState.selected*/) {
					r.push(d);
				}
			});
			return r;
		}, []);
	}

	getFirstSelectedCatalogItem() {
		return this.getCatalogs().reduce((r, catalog) => {
			return r.concat(catalog.descriptors.filter(d => SelectionManager.isSelected(d) /*d.uiState.selected*/));
		}, [])[0];
	}

	getCatalogItemById(id) {
		return this.getCatalogs().reduce((r, catalog) => {
			return r.concat(catalog.descriptors.filter(d => d.id === id));
		}, [])[0];
	}

	getCatalogItemByUid(uid) {
		return this.getCatalogs().reduce((r, catalog) => {
			return r.concat(catalog.descriptors.filter(d => UID.from(d) === uid));
		}, [])[0];
	}

	removeCatalogItem(deleteItem = {}) {
		this.getCatalogs().map(catalog => {
			catalog.descriptors = catalog.descriptors.filter(d => d.id !== deleteItem.id);
			return catalog;
		});
	}

	addNewItemToCatalog(newItem) {
		const id = guid();
		const type = newItem.uiState.type;
		newItem.id = id;
		UID.assignUniqueId(newItem.uiState);
		this.getCatalogs().filter(d => d.type === type).forEach(catalog => {
			catalog.descriptors.push(newItem);
		});
		// update indexes and integrate new model into catalog
		this.updateCatalogIndexes(this.getCatalogs());
		return this.getCatalogItemById(id);
	}

	updateCatalogIndexes(catalogs) {
		// associate catalog identity with individual descriptors so we can
		// update the catalog when any given descriptor is updated also add
		// vnfd model to the nsd object to make it easier to render an nsd
		const vnfdLookup = {};
		const updatedCatalogs = catalogs.map(catalog => {
			catalog.descriptors.map(descriptor => {
				if (typeof descriptor.meta === 'string' && descriptor.meta.trim() !== '') {
					try {
						descriptor.uiState = JSON.parse(descriptor.meta);
					} catch (ignore) {
						console.warn('Unable to deserialize the uiState property.');
					}
				} else if (typeof descriptor.meta === 'object') {
					descriptor.uiState = descriptor.meta;
					descriptor.meta = JSON.stringify(descriptor.meta);
				}

				const uiState = descriptor.uiState || (descriptor.uiState = {});
				uiState.catalogId = catalog.id;
				uiState.catalogName = catalog.name;
				uiState.type = catalog.type;
				if (!UID.hasUniqueId(uiState)) {
					UID.assignUniqueId(uiState);
				}
				if (catalog.type === 'vnfd') {
					vnfdLookup[descriptor.id] = descriptor;
				}
				return descriptor;
			});
			return catalog;
		});
		updatedCatalogs.filter(d => d.type === 'nsd').forEach(catalog =>  {
			catalog.descriptors = catalog.descriptors.map(descriptor => {
				const instanceRefCount = parseInt(descriptor.uiState['instance-ref-count'], 10);
				if (descriptor['constituent-vnfd']) {
					descriptor.vnfd = descriptor['constituent-vnfd'].map(d => {
						const vnfdId = d['vnfd-id-ref'];
						const vnfd = vnfdLookup[vnfdId];
						if (!vnfd) {
							throw new ReferenceError('no VNFD found in the VNFD Catalog for the constituent-vnfd: ' + d);
						}
						if (!isNaN(instanceRefCount) && instanceRefCount > 0) {
							// this will notify user that this item cannot be updated when/if they make a change to it
							vnfd.uiState['instance-ref-count'] = instanceRefCount;
						}
						// create an instance of this vnfd to carry transient ui state properties
						const instance = _.cloneDeep(vnfd);
						instance.uiState['member-vnf-index'] = d['member-vnf-index'];
						instance['vnf-configuration'] = d['vnf-configuration'];
						instance['start-by-default'] = d['start-by-default'];
						return instance;
					});
				}
				return descriptor;
			});
		});
		return updatedCatalogs;
	}

	updateCatalogItem(item) {
		// replace the given item in the catalog
		const catalogs = this.getCatalogs().map(catalog => {
			if (catalog.id === item.uiState.catalogId) {
				catalog.descriptors = catalog.descriptors.map(d => {
					if (d.id === item.id) {
						return item;
					}
					return d;
				});
			}
			return catalog;
		});
		this.setState({catalogs: catalogs});
	}

	mergeEditsIntoLatestFromServer(catalogsFromServer = []) {

		// if the UI has modified items use them instead of the server items

		const currentData = this.getCatalogs();

		const modifiedItemsMap = currentData.reduce((result, catalog) => {
			return result.concat(catalog.descriptors.filter(d => d.uiState.modified));
		}, []).reduce((r, d) => {
			r[d.uiState.catalogId + '/' + d.id] = d;
			return r;
		}, {});

		const itemMetaMap = currentData.reduce((result, catalog) => {
			return result.concat(catalog.descriptors.filter(d => d.uiState));
		}, []).reduce((r, d) => {
			r[d.uiState.catalogId + '/' + d.id] = d.uiState;
			return r;
		}, {});

		const newItemsMap = currentData.reduce((result, catalog) => {
			result[catalog.id] = catalog.descriptors.filter(d => d.uiState.isNew);
			return result;
		}, {});

		catalogsFromServer.forEach(catalog => {
			catalog.descriptors = catalog.descriptors.map(d => {
				const key = d.uiState.catalogId + '/' + d.id;
				if (modifiedItemsMap[key]) {
					// use local modified item instead of the server item
					return modifiedItemsMap[key];
				}
				if (itemMetaMap[key]) {
					Object.assign(d.uiState, itemMetaMap[key]);
				}
				return d;
			});
			if (newItemsMap[catalog.id]) {
				catalog.descriptors = catalog.descriptors.concat(newItemsMap[catalog.id]);
			}
		});

		return catalogsFromServer;

	}

	loadCatalogsSuccess(context) {
		const fromServer = this.updateCatalogIndexes(context.data);
		const catalogs = this.mergeEditsIntoLatestFromServer(fromServer);
		this.setState({
			catalogs: catalogs,
			isLoading: false
		});
	}

	deleteCatalogItemSuccess (response) {
		let catalogType = response.catalogType;
		let itemId = response.itemId;
		const catalogs = this.getCatalogs().map(catalog => {
			if (catalog.type === catalogType) {
				catalog.descriptors = catalog.descriptors.filter(d => d.id !== itemId);
			}
			return catalog;
		});

		this.setState({catalogs: catalogs});
	}

	deleteCatalogItemError (data) {
		console.log('Unable to delete', data.catalogType, 'id:', data.itemId, 'Error:', data.error.responseText);
		ComposerAppActions.showError.defer({
			errorMessage: 'Unable to delete ' + data.catalogType + ' id: ' + data.itemId + '. Check if it is in use'
		});
	}

	selectCatalogItem(item = {}) {
		SelectionManager.select(item);
	}

	catalogItemMetaDataChanged(item) {
		let requiresSave = false;
		const catalogs = this.getCatalogs().map(catalog => {
			if (catalog.id === item.uiState.catalogId) {
				catalog.descriptors = catalog.descriptors.map(d => {
					if (d.id === item.id) {
						// compare just the catalog uiState data (id, name, short-name, description, etc.)
						const modified = !areCatalogItemsMetaDataEqual(d, item);
						if (modified) {
							if (d.uiState['instance-ref-count'] > 0) {
								console.log('cannot edit NSD/VNFD with references to instantiated Network Services');
								ComposerAppActions.showError.defer({
									errorMessage: 'Cannot edit NSD/VNFD with references to instantiated Network Services'
								});
								return _.cloneDeep(d);
							} else {
								item.uiState.modified = modified;
								requiresSave = true;
								this.addSnapshot(item);
							}
						}
						return item;
					}
					return d;
				});
			}
			return catalog;
		});
		if (requiresSave) {
			this.setState({catalogs: catalogs, requiresSave: true});
		}
	}

	catalogItemDescriptorChanged(itemDescriptor) {
		// when a descriptor object is modified in the canvas we have to update the catalog
		const catalogId = itemDescriptor.uiState.catalogId;
		const catalogs = this.getCatalogs().map(catalog => {
			if (catalog.id === catalogId) {
				// find the catalog
				const descriptorId = itemDescriptor.id;
				// replace the old descriptor with the updated one
				catalog.descriptors = catalog.descriptors.map(d => {
					if (d.id === descriptorId) {
						if (d.uiState['instance-ref-count'] > 0) {
							console.log('cannot edit NSD/VNFD with references to instantiated Network Services');
							ComposerAppActions.showError.defer({
								errorMessage: 'Cannot edit NSD/VNFD with references to instantiated Network Services'
							});
							return _.cloneDeep(d);
						} else {
							itemDescriptor.model.uiState.modified = true;
							this.addSnapshot(itemDescriptor.model);
							return itemDescriptor.model;
						}
					}
					return d;
				});
			}
			return catalog;
		});
		this.setState({catalogs: catalogs, requiresSave: true})
	}

	deleteSelectedCatalogItem() {
		SelectionManager.getSelections().forEach(selectedId => {
			const item = this.getCatalogItemByUid(selectedId);
			if (item) {
				this.deleteCatalogItem(item);
			}
		});
		SelectionManager.clearSelectionAndRemoveOutline();
	}

	deleteCatalogItem(item) {
		const snapshot = JSON.stringify(item);
		function confirmDeleteCancel(event) {
			undo();
			event.preventDefault();
			ModalOverlayActions.hideModalOverlay();
		}
		const remove = () => {
			// item is deleted or does not exist on server, so remove from ui
			this.removeCatalogItem(item);
			this.setState({catalogs: this.getCatalogs()});
			const activeItem = ComposerAppStore.getState().item;
			if (activeItem && activeItem.id === item.id) {
				CatalogItemsActions.editCatalogItem.defer(null);
			}
			ModalOverlayActions.hideModalOverlay();
		};
		const undo = () => {
			// item failed to delete on server so revert ui
			const revertTo = JSON.parse(snapshot);
			this.updateCatalogItem(revertTo);
			const activeItem = ComposerAppStore.getState().item;
			if (activeItem && activeItem.id === revertTo.id) {
				SelectionManager.select(activeItem);
				CatalogItemsActions.editCatalogItem.defer(revertTo);
				SelectionManager.refreshOutline();
			}
		};
		if (item) {
			if (item.uiState.isNew) {
				CatalogDataStore.confirmDelete(remove, confirmDeleteCancel);
			} else {
				if (item.uiState['instance-ref-count'] > 0) {
					console.log('cannot delete NSD/VNFD with references to instantiated Network Services');
					ComposerAppActions.showError.defer({
						errorMessage: 'Cannot delete NSD/VNFD with references to instantiated Network Services'
					});
					undo();
				} else {
					const confirmDeleteOK = event => {
						event.preventDefault();
						item.uiState.deleted = true;
						this.setState({catalogs: this.getCatalogs()});
						ModalOverlayActions.showModalOverlay.defer();
						this.getInstance().deleteCatalogItem(item.uiState.type, item.id)
							.then(remove, undo)
							.then(ModalOverlayActions.hideModalOverlay, ModalOverlayActions.hideModalOverlay)
							.catch(function() {
								console.log('overcoming ES6 unhandled rejection red herring');
							});
					};
					CatalogDataStore.confirmDelete(confirmDeleteOK, confirmDeleteCancel);
				}
			}
		}
	}

	static confirmDelete(onClickYes, onClickCancel) {
		ModalOverlayActions.showModalOverlay.defer((
			<div className="actions panel">
				<div className="panel-header">
					<h1>Delete the selected catalog item?</h1>
				</div>
				<div className="panel-body">
					<a className="action confirm-yes primary-action Button" onClick={onClickYes}>Yes, delete selected catalog item</a>
					<a className="action cancel secondary-action Button" onClick={onClickCancel}>No, cancel</a>
				</div>
			</div>
		));
	}

	createCatalogItem(type = 'nsd') {
		const model = DescriptorModelMetaFactory.createModelInstanceForType(type);
		if (model) {
			const newItem = this.addNewItemToCatalog(model);
			newItem.uiState.isNew = true;
			newItem.uiState.modified = true;
			newItem.uiState['instance-ref-count'] = 0;
			// open the new model for editing in the canvas/details panels
			setTimeout(() => {
				this.selectCatalogItem(newItem);
				CatalogItemsActions.editCatalogItem.defer(newItem);
			}, 200);
		}
	}

	duplicateSelectedCatalogItem() {
		const item = this.getFirstSelectedCatalogItem();
		if (item) {
			const newItem = _.cloneDeep(item);
			newItem.name = newItem.name + ' Copy';
			const nsd = this.addNewItemToCatalog(newItem);
			this.selectCatalogItem(nsd);
			nsd.uiState.isNew = true;
			nsd.uiState.modified = true;
			nsd.uiState['instance-ref-count'] = 0;
			// note duplicated items get a new id, map the layout position
			// of the old id to the new id in order to preserve the layout
			if (nsd.uiState.containerPositionMap) {
				nsd.uiState.containerPositionMap[nsd.id] = nsd.uiState.containerPositionMap[item.id];
				delete nsd.uiState.containerPositionMap[item.id];
			}
			setTimeout(() => {
				this.selectCatalogItem(nsd);
				CatalogItemsActions.editCatalogItem.defer(nsd);
			}, 200);
		}
	}

	addSnapshot(item) {
		if (item) {
			if (!this.snapshots[item.id]) {
				this.snapshots[item.id] = [];
			}
			this.snapshots[item.id].push(JSON.stringify(item));
		}
	}

	resetSnapshots(item) {
		if (item) {
			this.snapshots[item.id] = [];
			this.addSnapshot(item);
		}
	}

	editCatalogItem(item) {
		if (item) {
			this.addSnapshot(item);
			// replace the given item in the catalog
			const catalogs = this.getCatalogs().map(catalog => {
				catalog.descriptors = catalog.descriptors.map(d => {
					// note only one item can be "open" at a time
					// so remove the flag from all the other items
					d.uiState.isOpenForEdit = (d.id === item.id);
					if (d.uiState.isOpenForEdit) {
						return item;
					}
					return d;
				});
				return catalog;
			});
			this.setState({catalogs: catalogs});
			this.catalogItemMetaDataChanged(item);
		}
	}

	cancelCatalogItemChanges() {
		const activeItem = ComposerAppStore.getState().item;
		if (activeItem) {
			const snapshots = this.snapshots[activeItem.id];
			if (snapshots.length) {
				const revertTo = JSON.parse(snapshots[0]);
				this.updateCatalogItem(revertTo);
				// TODO should the cancel action clear the undo/redo stack back to the beginning?
				this.resetSnapshots(revertTo);
				this.setState({requiresSave: false});
				CatalogItemsActions.editCatalogItem.defer(revertTo);
			}
		}
	}

	saveCatalogItem() {
		const activeItem = ComposerAppStore.getState().item;
		if (activeItem) {
			if (activeItem.uiState['instance-ref-count'] > 0) {
				console.log('cannot save NSD/VNFD with references to instantiated Network Services');
				ComposerAppActions.showError.defer({
					errorMessage: 'Cannot save NSD/VNFD with references to instantiated Network Services'
				});
				return;
			}
			const success = () => {
				delete activeItem.uiState.isNew;
				delete activeItem.uiState.modified;
				this.updateCatalogItem(activeItem);
				// TODO should the save action clear the undo/redo stack back to the beginning?
				this.resetSnapshots(activeItem);
				ModalOverlayActions.hideModalOverlay.defer();
				CatalogItemsActions.editCatalogItem.defer(activeItem);
			};
			const failure = () => {
				ModalOverlayActions.hideModalOverlay.defer();
				CatalogItemsActions.editCatalogItem.defer(activeItem);
			};
			const exception = () => {
				console.warn('unable to save catalog item', activeItem);
				ModalOverlayActions.hideModalOverlay.defer();
				CatalogItemsActions.editCatalogItem.defer(activeItem);
			};
			ModalOverlayActions.showModalOverlay.defer();
			this.getInstance().saveCatalogItem(activeItem).then(success, failure).catch(exception);
		}
	}

	exportSelectedCatalogItems(draggedItem) {
		const onSelectFormat = (selectedFormat, event) => {
			CatalogPackageManagerActions.downloadCatalogPackage.defer({
				selectedItems: selectedItems,
				selectedFormat: selectedFormat
			});
			ModalOverlayActions.hideModalOverlay();
		};

		const onCancel = () => {
			ModalOverlayActions.hideModalOverlay();
		};

		if (draggedItem) {
			// if item is given make sure it is also selected
			//draggedItem.uiState.selected = true;
			SelectionManager.addSelection(draggedItem);
			this.updateCatalogItem(draggedItem);
		}
		// collect the selected items and delegate to the catalog package manager action creator
		const selectedItems = this.getAllSelectedCatalogItems();
		if (selectedItems.length) {
			CatalogDataStore.chooseExportFormat(onSelectFormat, onCancel);
		}
	}

	static chooseExportFormat(onSelectFormat, onCancel) {
		let dialogListElements = [];

		dialogListElements.push(
			<li>
				Select a descriptor format:
			</li>
		);

		defaults.catalogItemExportFormats.map((format) => {
			let classNames = 'action Button';
			if (format == 'mano') {
				classNames += ' primary-action';
			} else {
				classNames += ' secondary-action';
			}

			dialogListElements.push(
				<li>
					<a className={classNames} onClick={onSelectFormat.bind(this, format)} style={{textTransform: 'uppercase'}}>{format}</a>
				</li>
			);
		});

		dialogListElements.push(
			<li>
				<a className="action Button" onClick={onCancel} style={{textTransform: 'capitalize', marginTop: '4rem', float: 'right'}}>Cancel</a>
			</li>
		)

		ModalOverlayActions.showModalOverlay.defer((
			<div className="actions panel">
				<div className="panel-header" style={{minWidth: '30rem'}}>
					<h1>Export Catalog Item</h1>
				</div>
				<div className="panel-body">
					<ul>
						{dialogListElements}
					</ul>
				</div>
			</div>
		));
	}

}

export default alt.createStore(CatalogDataStore, 'CatalogDataStore');
