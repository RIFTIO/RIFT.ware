
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import d3 from 'd3'
import alt from '../alt'
import UID from '../libraries/UniqueId'
import DescriptorModelFactory from '../libraries/model/DescriptorModelFactory'
import PanelResizeAction from '../actions/PanelResizeAction'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import CanvasEditorActions from '../actions/CanvasEditorActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import CatalogFilterActions from '../actions/CatalogFilterActions'
import CanvasPanelTrayActions from '../actions/CanvasPanelTrayActions'
import SelectionManager from '../libraries/SelectionManager'
import CatalogDataStore from '../stores/CatalogDataStore'
import isFullScreen from '../libraries/isFullScreen'

const getDefault = (name, defaultValue) => {
	const val = window.localStorage.getItem('defaults-' + name);
	if (val) {
		if (_.isNumber(val)) {
			if (val < 0) {
				return setDefault(name, 0);
			}
		}
		return Number(val);
	}
	setDefault(name, defaultValue);
	return defaultValue;
};

const setDefault = (name, defaultValue) => {
	window.localStorage.setItem('defaults-' + name, defaultValue);
	return defaultValue;
};

/* the top and bottom positions are managed by css; requires div to be display: absolute*/
const defaults = {
	left: getDefault('catalog-panel-start-width', 300),
	right: getDefault('details-panel-start-width', 365),
	bottom: 25 + getDefault('defaults-forwarding-graphs-panel-start-height', 0),
	showMore: false,
	zoom: getDefault('zoom', 100),
	filterCatalogBy: 'nsd',
	defaultPanelTrayOpenZoom: (() => {
		let zoom = parseFloat(getDefault('panel-tray-zoom', 75));
		if (isNaN(zoom)) {
			zoom = 75;
		}
		zoom = Math.min(100, zoom);
		zoom = Math.max(25, zoom);
		setDefault('panel-tray-zoom', zoom);
		return zoom;
	})()
};

const autoZoomCanvasScale = d3.scale.linear().domain([0, 300]).range([100, 50]).clamp(true);

const uiTransientState = {};

class ComposerAppStore {

	constructor() {
		// the catalog item currently being edited in the composer
		this.item = null;
		// the left and right sides of the canvas area
		this.layout = {
			left: defaults.left,
			right: defaults.right,
			bottom: defaults.bottom
		};
		uiTransientState.restoreLayout = this.layout;
		this.zoom = defaults.zoom;
		this.showMore = defaults.showMore;
		this.filterCatalogByTypeValue = defaults.filterCatalogBy;
		// transient ui state
		this.drag = null;
		this.message = '';
		this.messageType = '';
		this.showJSONViewer = false;
		this.showClassifiers = {};
		this.editPathsMode = false;
		this.fullScreenMode = false;
		this.bindListeners({
			onResize: PanelResizeAction.RESIZE,
			editCatalogItem: CatalogItemsActions.EDIT_CATALOG_ITEM,
			catalogItemMetaDataChanged: CatalogItemsActions.CATALOG_ITEM_META_DATA_CHANGED,
			catalogItemDescriptorChanged: CatalogItemsActions.CATALOG_ITEM_DESCRIPTOR_CHANGED,
			toggleShowMoreInfo: CanvasEditorActions.TOGGLE_SHOW_MORE_INFO,
			showMoreInfo: CanvasEditorActions.SHOW_MORE_INFO,
			showLessInfo: CanvasEditorActions.SHOW_LESS_INFO,
			applyDefaultLayout: CanvasEditorActions.APPLY_DEFAULT_LAYOUT,
			addVirtualLinkDescriptor: CanvasEditorActions.ADD_VIRTUAL_LINK_DESCRIPTOR,
			addForwardingGraphDescriptor: CanvasEditorActions.ADD_FORWARDING_GRAPH_DESCRIPTOR,
			addVirtualDeploymentDescriptor: CanvasEditorActions.ADD_VIRTUAL_DEPLOYMENT_DESCRIPTOR,
			selectModel: ComposerAppActions.SELECT_MODEL,
			outlineModel: ComposerAppActions.OUTLINE_MODEL,
			showError: ComposerAppActions.SHOW_ERROR,
			clearError: ComposerAppActions.CLEAR_ERROR,
			setDragState: ComposerAppActions.SET_DRAG_STATE,
			filterCatalogByType: CatalogFilterActions.FILTER_BY_TYPE,
			setCanvasZoom: CanvasEditorActions.SET_CANVAS_ZOOM,
			showJsonViewer: ComposerAppActions.SHOW_JSON_VIEWER,
			closeJsonViewer: ComposerAppActions.CLOSE_JSON_VIEWER,
			toggleCanvasPanelTray: CanvasPanelTrayActions.TOGGLE_OPEN_CLOSE,
			openCanvasPanelTray: CanvasPanelTrayActions.OPEN,
			closeCanvasPanelTray: CanvasPanelTrayActions.CLOSE,
			enterFullScreenMode: ComposerAppActions.ENTER_FULL_SCREEN_MODE,
			exitFullScreenMode: ComposerAppActions.EXIT_FULL_SCREEN_MODE
		});
	}

	onResize(e) {
		if (e.type === 'resize-manager.resize.catalog-panel') {
			const layout = Object.assign({}, this.layout);
			layout.left = Math.max(0, layout.left - e.moved.x);
			if (layout.left !== this.layout.left) {
				this.setState({layout: layout});
			}
		} else if (e.type === 'resize-manager.resize.details-panel') {
			const layout = Object.assign({}, this.layout);
			layout.right = Math.max(0, layout.right + e.moved.x);
			if (layout.right !== this.layout.right) {
				this.setState({layout: layout});
			}
		} else if (/^resize-manager\.resize\.canvas-panel-tray/.test(e.type)) {
			const layout = Object.assign({}, this.layout);
			layout.bottom = Math.max(25, layout.bottom + e.moved.y);
			if (layout.bottom !== this.layout.bottom) {
				const zoom = autoZoomCanvasScale(layout.bottom) ;
				if (this.zoom !== zoom) {
					this.setState({layout: layout, zoom: zoom});
				} else {
					this.setState({layout: layout});
				}
			}
		} else if (e.type !== 'resize') {
			console.log('no resize handler for ', e.type, '. Do you need to add a handler in ComposerAppStore::onResize()?')
		}
		SelectionManager.refreshOutline();
	}

	updateItem(item) {
		if(!document.body.classList.contains('resizing')) {
			this.setState({item: _.cloneDeep(item)});
		}
		SelectionManager.refreshOutline();
	}

	editCatalogItem(item) {
		if (item && item.uiState) {
			item.uiState.isOpenForEdit = true;
			if (item.uiState.type !== 'nsd') {
				this.closeCanvasPanelTray();
			}
		}
		SelectionManager.select(item);
		this.updateItem(item);
	}

	catalogItemMetaDataChanged(item) {
		this.updateItem(item);
	}

	catalogItemDescriptorChanged(itemDescriptor) {
		this.catalogItemMetaDataChanged(itemDescriptor.model);
	}

	showMoreInfo() {
		this.setState({showMore: true});
	}

	showLessInfo() {
		this.setState({showMore: false});
	}

	showError(data) {
		this.setState({message: data.errorMessage, messageType: 'error'});
	}

	clearError() {
		this.setState({message: '', messageType: ''});
	}

	toggleShowMoreInfo() {
		this.setState({showMore: !this.showMore});
	}

	applyDefaultLayout() {
		if (this.item && this.item.uiState && this.item.uiState.containerPositionMap) {
			if (!_.isEmpty(this.item.uiState.containerPositionMap)) {
				this.item.uiState.containerPositionMap = {};
				CatalogItemsActions.catalogItemMetaDataChanged.defer(this.item);
			}
		}
	}

	addVirtualLinkDescriptor(dropCoordinates = null) {
		let vld;
		if (this.item) {
			if (this.item.uiState.type === 'nsd') {
				const nsdc = DescriptorModelFactory.newNetworkService(this.item);
				vld = nsdc.createVld();
			} else if (this.item.uiState.type === 'vnfd') {
				const vnfd = DescriptorModelFactory.newVirtualNetworkFunction(this.item);
				vld = vnfd.createVld();
			}
			if (vld) {
				vld.uiState.dropCoordinates = dropCoordinates;
				SelectionManager.clearSelectionAndRemoveOutline();
				SelectionManager.addSelection(vld);
				this.updateItem(vld.getRoot().model);
				CatalogItemsActions.catalogItemDescriptorChanged.defer(vld.getRoot());
			}
		}
	}

	addForwardingGraphDescriptor(dropCoordinates = null) {
		if (this.item && this.item.uiState.type === 'nsd') {
			const nsdc = DescriptorModelFactory.newNetworkService(this.item);
			const fg = nsdc.createVnffgd();
			fg.uiState.dropCoordinates = dropCoordinates;
			SelectionManager.clearSelectionAndRemoveOutline();
			SelectionManager.addSelection(fg);
			this.updateItem(nsdc.model);
			CatalogItemsActions.catalogItemDescriptorChanged.defer(nsdc);
		}
	}

	addVirtualDeploymentDescriptor(dropCoordinates = null) {
		if (this.item.uiState.type === 'vnfd') {
			const vnfd = DescriptorModelFactory.newVirtualNetworkFunction(this.item);
			const vdu = vnfd.createVdu();
			vdu.uiState.dropCoordinates = dropCoordinates;
			SelectionManager.clearSelectionAndRemoveOutline();
			SelectionManager.addSelection(vdu);
			this.updateItem(vdu.getRoot().model);
			CatalogItemsActions.catalogItemDescriptorChanged.defer(vdu.getRoot());
		}
	}

	selectModel(container) {
		if (SelectionManager.select(container)) {
			const model = DescriptorModelFactory.isContainer(container) ? container.getRoot().model : container;
			this.catalogItemMetaDataChanged(model);
		}
	}

	outlineModel(obj) {
		const uid = UID.from(obj);
		requestAnimationFrame(() => {
			SelectionManager.outline(Array.from(document.querySelectorAll(`[data-uid="${uid}"]`)));
		});
	}

	clearSelection() {
		SelectionManager.clearSelectionAndRemoveOutline();
		this.catalogItemMetaDataChanged(this.item);
	}

	setDragState(dragState) {
		this.setState({drag: dragState});
	}

	filterCatalogByType(typeValue) {
		this.setState({filterCatalogByTypeValue: typeValue})
	}

	setCanvasZoom(zoom) {
		this.setState({zoom: zoom});
	}

	showJsonViewer() {
		this.setState({showJSONViewer: true});
	}

	closeJsonViewer() {
		this.setState({showJSONViewer: false});
	}

	toggleCanvasPanelTray() {
		const layout = this.layout;
		if (layout.bottom > 25) {
			this.closeCanvasPanelTray();
		} else {
			this.openCanvasPanelTray();
		}
	}

	openCanvasPanelTray() {
		const layout = {
			left: this.layout.left,
			right: this.layout.right,
			bottom: 300
		};
		const zoom = defaults.defaultPanelTrayOpenZoom;
		if (this.zoom !== zoom) {
			this.setState({layout: layout, zoom: zoom, restoreZoom: this.zoom});
		} else {
			this.setState({layout: layout});
		}
	}

	closeCanvasPanelTray() {
		const layout = {
			left: this.layout.left,
			right: this.layout.right,
			bottom: 25
		};
		const zoom = this.restoreZoom || autoZoomCanvasScale(layout.bottom);
		if (this.zoom !== zoom) {
			this.setState({layout: layout, zoom: zoom, restoreZoom: null});
		} else {
			this.setState({layout: layout, restoreZoom: null});
		}
	}

	enterFullScreenMode() {

		/**
		 * https://developer.mozilla.org/en-US/docs/Web/API/Fullscreen_API
		 * This is an experimental api but works our target browsers and ignored by others
		 */
		const eventNames = ['fullscreenchange', 'mozfullscreenchange', 'webkitfullscreenchange', 'msfullscreenchange'];

		const appRoot = document.body;//.getElementById('RIFT_wareLaunchpadComposerAppRoot');

		const comp = this;

		function onFullScreenChange() {

			if (isFullScreen()) {
				const layout = comp.layout;
				const restoreLayout = _.cloneDeep(layout);
				uiTransientState.restoreLayout = restoreLayout;
				layout.left = 0;
				layout.right = 0;
				comp.setState({fullScreenMode: true, layout: layout, restoreLayout: restoreLayout});
			} else {
				comp.setState({fullScreenMode: false, layout: uiTransientState.restoreLayout});
			}

		}

		if (this.fullScreenMode === false) {

			if (appRoot.requestFullscreen) {
				appRoot.requestFullscreen();
			} else if (appRoot.msRequestFullscreen) {
				appRoot.msRequestFullscreen();
			} else if (appRoot.mozRequestFullScreen) {
				appRoot.mozRequestFullScreen();
			} else if (appRoot.webkitRequestFullscreen) {
				appRoot.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
			}

			eventNames.map(name => {
				document.removeEventListener(name, onFullScreenChange);
				document.addEventListener(name, onFullScreenChange);
			});

		}

	}

	exitFullScreenMode() {

		if (document.exitFullscreen) {
			document.exitFullscreen();
		} else if (document.msExitFullscreen) {
			document.msExitFullscreen();
		} else if (document.mozCancelFullScreen) {
			document.mozCancelFullScreen();
		} else if (document.webkitExitFullscreen) {
			document.webkitExitFullscreen();
		}

		this.setState({fullScreenMode: false});

	}

}

export default alt.createStore(ComposerAppStore, 'ComposerAppStore');
