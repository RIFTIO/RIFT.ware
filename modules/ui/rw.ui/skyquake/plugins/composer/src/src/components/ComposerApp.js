/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

window['RIFT_wareLaunchpadComposerVersion'] = `semver 0.0.79`;

import 'es5-shim'
import 'babel-polyfill'
import alt from '../alt'
import UID from '../libraries/UniqueId'
import utils from '../libraries/utils'
import React from 'react'
import ReactDOM from 'react-dom'
import Crouton from 'react-crouton'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import DeletionManager from '../libraries/DeletionManager'
import SelectionManager from '../libraries/SelectionManager'
import ResizableManager from '../libraries/ResizableManager'
import DescriptorModelFactory from '../libraries/model/DescriptorModelFactory'
import RiftHeader from './RiftHeader'
import CanvasPanel from './CanvasPanel'
import CatalogPanel from './CatalogPanel'
import DetailsPanel from './DetailsPanel'
import ModalOverlay from './ModalOverlay'
import ComposerAppToolbar from './ComposerAppToolbar'
import PanelResizeAction from '../actions/PanelResizeAction'
import ComposerAppActions from '../actions/ComposerAppActions'
import ComposerAppStore from '../stores/ComposerAppStore'
import CatalogDataStore from '../stores/CatalogDataStore'
import TooltipManager from '../libraries/TooltipManager'
import CatalogItemsActions from '../actions/CatalogItemsActions'

import 'normalize.css'
import '../styles/AppRoot.scss'

const resizeManager = new ResizableManager(window);

const clearLocalStorage = utils.getSearchParams(window.location).hasOwnProperty('clearLocalStorage');

const preventDefault = e => e.preventDefault();
const clearDragState = () => ComposerAppActions.setDragState(null);

const ComposerApp = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return ComposerAppStore.getState();
	},
	getDefaultProps() {
		return {};
	},
	componentWillMount() {
		if (clearLocalStorage) {
			window.localStorage.clear();
		}
		this.state.isLoading = CatalogDataStore.getState().isLoading;
		ComposerAppStore.listen(this.onChange);
		CatalogDataStore.listen(this.onCatalogDataChanged);
		window.addEventListener('resize', this.resize);
		window.onbeforeunload = this.onBeforeUnload;
		// prevent browser from downloading any drop outside of our specific drop zones
		window.addEventListener('dragover', preventDefault);
		window.addEventListener('drop', preventDefault);
		// ensure drags initiated in the app clear the state on drop
		window.addEventListener('drop', clearDragState);
		DeletionManager.addEventListeners();
	},
	componentWillUnmount() {
		window.removeEventListener('resize', this.resize);
		window.removeEventListener('dragover', preventDefault);
		window.removeEventListener('drop', preventDefault);
		window.removeEventListener('drop', clearDragState);
		// resizeManager automatically registered its event handlers
		resizeManager.removeAllEventListeners();
		ComposerAppStore.unlisten(this.onChange);
		CatalogDataStore.unlisten(this.onCatalogDataChanged);
		DeletionManager.removeEventListeners();
		TooltipManager.removeEventListeners();
	},
	componentDidMount() {
		resizeManager.addAllEventListeners();
		const snapshot = window.localStorage.getItem('composer');
		if (snapshot) {
			alt.bootstrap(snapshot);
		}
		document.body.addEventListener('keydown', (event) => {
			// prevent details editor form from blowing up the app
			const ENTER_KEY = 13;
			if (event.which === ENTER_KEY) {
				event.preventDefault();
				return false;
			}
		});
		const appRootElement = ReactDOM.findDOMNode(this.refs.appRoot);
		TooltipManager.addEventListeners(appRootElement);
		SelectionManager.onClearSelection = () => {
			if (this.state.item) {
				CatalogItemsActions.catalogItemMetaDataChanged.defer(this.state.item);
			}
		};
	},
	componentDidUpdate() {
		if (this.state.fullScreenMode) {
			document.body.classList.add('-is-full-screen');
		} else {
			document.body.classList.remove('-is-full-screen');
		}
		SelectionManager.refreshOutline();
	},
	resize(e) {
		PanelResizeAction.resize(e);
	},
	render() {

		function onClickUpdateSelection(event) {
			if (event.defaultPrevented) {
				return
			}
			const element = SelectionManager.getClosestElementWithUID(event.target);
			if (element) {
				SelectionManager.select(element);
				SelectionManager.refreshOutline();
				event.preventDefault();
			} else {
				SelectionManager.clearSelectionAndRemoveOutline();
			}
		}

		let cpNumber = 0;
		let AppHeader = (<div className="AppHeader">
							<RiftHeader />
						</div>);
		// AppHeader = null;
		const classNames = ClassNames('ComposerApp');
		const isNew = this.state.item && this.state.item.uiState.isNew;
		const hasItem = this.state.item && this.state.item.uiState;
		const isModified = this.state.item && this.state.item.uiState.modified;
		const isEditingNSD = this.state.item && this.state.item.uiState && /nsd/.test(this.state.item.uiState.type);
		const isEditingVNFD = this.state.item && this.state.item.uiState && /vnfd/.test(this.state.item.uiState.type);
		const containers = [this.state.item].reduce(DescriptorModelFactory.buildCatalogItemFactory(CatalogDataStore.getState().catalogs), []);

		containers.filter(d => DescriptorModelFactory.isConnectionPoint(d)).forEach(d => {
			d.cpNumber = ++cpNumber;
			containers.filter(d => DescriptorModelFactory.isVnfdConnectionPointRef(d)).filter(ref => ref.key === d.key).forEach(ref => ref.cpNumber = d.cpNumber);
		});
		const canvasTitle = containers.length ? containers[0].model.name : '';
		const hasNoCatalogs = CatalogDataStore.getState().catalogs.length === 0;
		const isLoading = this.state.isLoading;

		return (
			<div ref="appRoot" id="RIFT_wareLaunchpadComposerAppRoot" className="AppRoot" onClick={onClickUpdateSelection}>
				<i className="corner-accent top left" />
				<i className="corner-accent top right" />
				<i className="corner-accent bottom left" />
				<i className="corner-accent bottom right" />
				{AppHeader}
				<Crouton id={Date.now()} type={this.state.messageType} message={this.state.message} onDismiss={ComposerAppActions.clearError} />
				<div className="AppBody">
					<div className={classNames}>
						<CatalogPanel layout={this.state.layout}
									  isLoading={isLoading}
									  hasNoCatalogs={hasNoCatalogs}
									  filterByType={this.state.filterCatalogByTypeValue} />
						<CanvasPanel layout={this.state.layout}
									 hasNoCatalogs={hasNoCatalogs}
									 showMore={this.state.showMore}
									 containers={containers}
									 title={canvasTitle}
									 zoom={this.state.zoom} />
						<DetailsPanel layout={this.state.layout}
									  hasNoCatalogs={hasNoCatalogs}
									  showMore={this.state.showMore}
									  containers={containers}
									  showJSONViewer={this.state.showJSONViewer} />
						<ComposerAppToolbar layout={this.state.layout}
											showMore={this.state.showMore}
											isEditingNSD={isEditingNSD}
											isEditingVNFD={isEditingVNFD}
											isModified={isModified}
											isNew={isNew}
											disabled={!hasItem}
											onClick={event => event.stopPropagation()}/>
					</div>
				</div>
				<ModalOverlay />
			</div>
		);

	},
	onChange(state) {
		this.setState(state);
	},
	onCatalogDataChanged(catalogDataState) {
		const catalogs = catalogDataState.catalogs;
		const unsavedChanges = catalogs.reduce((result, catalog) => {
			if (result) {
				return result;
			}
			return catalog.descriptors.reduce((result, descriptor) => {
				if (result) {
					return result;
				}
				return descriptor.uiState.modified;
			}, false);
		}, false);
		this.setState({
			unsavedChanges: unsavedChanges,
			isLoading: catalogDataState.isLoading
		});
	},
	onBeforeUnload() {
		// https://trello.com/c/c8v321Xx/160-prompt-user-to-save-changes
		//const snapshot = alt.takeSnapshot();
		//window.localStorage.setItem('composer', snapshot);
		if (this.state.unsavedChanges) {
			return 'You have unsaved changes. If you do not onboard (or update) your changes they will be lost.';
		}
	}

});

export default ComposerApp;
