
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
'use strict';

import _ from 'lodash'
import $ from 'jquery'
import alt from '../alt'
import utils from '../libraries/utils'
import serializers from '../libraries/model/DescriptorModelSerializer'
import CatalogDataSourceActions from '../actions/CatalogDataSourceActions'
let Utils = require('utils/utils.js');
const CatalogDataSource = {

	loadCatalogs: function() {
		return {
			remote: function() {
				return new Promise(function(resolve, reject) {
					$.ajax({
						beforeSend: Utils.addAuthorizationStub,
						url: '//' + window.location.hostname + ':3000/launchpad/catalog?api_server=' + utils.getSearchParams(window.location).api_server,
						//url: 'assets/big-honking-catalog.json',
						//url: 'assets/ping-pong-catalog.json',
						//url: 'assets/empty-nsd-catalog.json',
						success: function(data) {
							if (typeof data == 'string') {
								data = JSON.parse(data);
							}
							const context = Object.assign({}, this, {data: data});
							resolve(context);
						},
						error: function(error) {
							if (typeof error == 'string') {
								error = JSON.parse(error);
							}
							reject(error);
						}
					}).fail(function(xhr){
			            //Authentication and the handling of fail states should be wrapped up into a connection class.
			            Utils.checkAuthentication(xhr.status);
		          	});
				});
			},
			success: CatalogDataSourceActions.loadCatalogsSuccess,
			error: CatalogDataSourceActions.loadCatalogsError
		}
	},

	deleteCatalogItem: function() {
		return {
			remote: function(state, catalogType, itemId) {
				return new Promise(function(resolve, reject) {
					$.ajax({
						url: '//' + window.location.hostname + ':3000/launchpad/catalog/' + catalogType + '/' + itemId + '?api_server=' + utils.getSearchParams(window.location).api_server,
						type: 'DELETE',
						beforeSend: utils.addAuthorizationStub,
						success: function(data) {
							resolve({
								data: data,
								catalogType: catalogType,
								itemId: itemId
							});
						},
						error: function(error) {
							reject({
								error: error,
								catalogType: catalogType,
								itemId: itemId
							});
						}
					});
				});
			},
			success: CatalogDataSourceActions.deleteCatalogItemSuccess,
			error: CatalogDataSourceActions.deleteCatalogItemError
		}
	},

	saveCatalogItem: function () {
		return {
			remote: function (state, item = {}) {
				const method = item.uiState.isNew ? 'POST' : 'PUT';
				const payload = serializers.serialize(item);
				return new Promise((resolve, reject) => {
					if (payload === false) {
						reject({
							error: 'unable to serialize item: ' + item.id,
							catalogType: item.uiState.type,
							itemId: payload.id
						});
					}
					if (method === 'POST') {
						$.ajax({
							url: '//' + window.location.hostname + ':3000/launchpad/catalog/' + item.uiState.type + '?api_server=' + utils.getSearchParams(window.location).api_server,
							type: method,
							beforeSend: utils.addAuthorizationStub,
							data: payload,
							dataType: 'json',
							success: function(data) {
								resolve({
									data: data,
									catalogType: item.uiState.type,
									itemId: payload.id
								});
							},
							error: function(error) {
								reject({
									error: error,
									catalogType: item.uiState.type,
									itemId: payload.id
								});
							}
						});
					} else {
						$.ajax({
							url: '//' + window.location.hostname + ':3000/launchpad/catalog/' + item.uiState.type + '/' + payload.id + '?api_server=' + utils.getSearchParams(window.location).api_server,
							type: method,
							beforeSend: utils.addAuthorizationStub,
							data: payload,
							dataType: 'json',
							success: function(data) {
								resolve({
									data: data,
									catalogType: item.uiState.type,
									itemId: payload.id
								});
							},
							error: function(error) {
								reject({
									error: error,
									catalogType: item.uiState.type,
									itemId: payload.id
								});
							}
						});
					}
				});
			},
			success: CatalogDataSourceActions.saveCatalogItemSuccess,
			error: CatalogDataSourceActions.saveCatalogItemError
		};
	}
};

export default CatalogDataSource;
