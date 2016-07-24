
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import UID from '../libraries/UniqueId'
import React from 'react'
import messages from './messages'
import ClassNames from 'classnames'
import PureRenderMixin from 'react-addons-pure-render-mixin'
import CatalogDataStore from '../stores/CatalogDataStore'
import CatalogItemsActions from '../actions/CatalogItemsActions'
import ComposerAppActions from '../actions/ComposerAppActions'
import SelectionManager from '../libraries/SelectionManager'

import '../styles/CatalogItems.scss'
import imgFile from 'file!../images/vendor-riftio.png'

const CatalogItems = React.createClass({
	mixins: [PureRenderMixin],
	getInitialState() {
		return CatalogDataStore.getState();
	},
	getDefaultProps() {
		return {
			filterByType: 'nsd'
		};
	},
	componentWillMount() {
		CatalogDataStore.listen(this.onChange);
	},
	componentDidMount() {
		// async actions creator will dispatch loadCatalogsSuccess and loadCatalogsError messages
		CatalogDataStore.loadCatalogs().catch(e => console.warn('unable to load catalogs', e));
	},
	componentWillUnmount() {
		CatalogDataStore.unlisten(this.onChange);
	},
	onChange(state) {
		this.setState(state);
	},
	renderVersion(version) {
		if (version) {
			return (<span className='version'>{version}</span>);
		} // else return null by default
	},
	handleImageError(e) {
		console.log('Bad logo path, using default');
		e.target.src = require('style/img/catalog-default.svg');
	},
	render() {
		const self = this;
		const onDragStart = function(event) {
			const data = {type: 'catalog-item', item: this};
			event.dataTransfer.effectAllowed = 'copy';
			event.dataTransfer.setData('text', JSON.stringify(data));
			ComposerAppActions.setDragState(data);
		};
		const onDblClickCatalogItem = function () {
			CatalogItemsActions.editCatalogItem(this);
		};
		const onClickCatalogItem = function () {
			// single clicking an item is handled by ComposerApp::onClick handler
			//CatalogItemsActions.selectCatalogItem(this);
		};
		const cleanDataURI = this.cleanDataURI;
		const items = this.getCatalogItems().map(function (d) {
			const isNSD = d.uiState.type === 'nsd';
			const isVNFD = d.uiState.type === 'vnfd';
			const isDeleted = d.uiState.deleted;
			const isModified = d.uiState.modified;
			const isSelected = SelectionManager.isSelected(d);
			const isOpenForEdit = d.uiState.isOpenForEdit;
			const spanClassNames = ClassNames({'-is-selected': isSelected, '-is-open-for-edit': isOpenForEdit});
			const sectionClassNames = ClassNames('catalog-item', {'-is-modified': isModified, '-is-deleted': isDeleted});
			const instanceCount = d.uiState['instance-ref-count'];
			const instanceCountLabel = isNSD && instanceCount ? <span>({instanceCount})</span> : null;
			let type;
			if(isNSD) {
				type = 'nsd';
			}
			if(isVNFD) {
				type = 'vnfd';
			}
			return (
				<li key={d.id} data-uid={UID.from(d)} onClick={onClickCatalogItem.bind(d)} onDoubleClick={onDblClickCatalogItem.bind(d)}>
					<div className={spanClassNames}>
						<div className={sectionClassNames} id={d.id} draggable="true" onDragStart={onDragStart.bind(d)}>
							{isModified ? <div className="-is-modified-indicator" title="This descriptor has changes."></div> : null}
							<dl>
								<dt className="name">{d.name} {instanceCountLabel}</dt>
								<dd className="logo">
								<img className="logo" src={cleanDataURI(d['logo'], type, d.id)} draggable="false"  onError={self.handleImageError} />
								</dd>
								<dd className="short-name" title={d.name}>{d['short-name']}</dd>
								<dd className="description">{d.description}</dd>
								<dd className="vendor">{d.vendor || d.provider} {self.renderVersion(d.version)}</dd>
							</dl>
						</div>
					</div>
					{isOpenForEdit ? <div className="-is-open-for-edit-indicator" title="This descriptor is open in the canvas."></div> : null}
				</li>
			);
		});
		return (
			<div className="CatalogItems" data-offset-parent="true">
				<ul>
					{items.length ? items : messages.catalogWelcome}
				</ul>
			</div>
		);
	},
	cleanDataURI(imageString, type, id) {
		if (/\bbase64\b/g.test(imageString)) {
			return imageString;
		} else if (/<\?xml\b/g.test(imageString)) {
			const imgStr = imageString.substring(imageString.indexOf('<?xml'));
			return 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(imgStr);
		} else if (/\.(svg|png|gif|jpeg|jpg)$/.test(imageString)) {
			return 'assets/logos/' + type + '/' + id + '/' + imageString;
			// return require('../images/logos/' + imageString);
		}
			return require('style/img/catalog-default.svg');
	},
	getCatalogItems() {
		const catalogFilter = (d) => {return d.type === this.props.filterByType};
		return this.state.catalogs.filter(catalogFilter).reduce((result, catalog) => {
			return result.concat(catalog.descriptors);
		}, []);
	}
});

export default CatalogItems;
