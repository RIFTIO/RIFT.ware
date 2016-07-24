/**
 * Created by onvelocity on 2/11/16.
 */
'use strict';

import d3 from 'd3'
import SelectionManager from '../SelectionManager.js'
import DescriptorModelFactory from '../model/DescriptorModelFactory'
import DescriptorGraphSelection from './DescriptorGraphSelection'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import GraphVirtualNetworkFunction from './GraphVirtualNetworkFunction'
import GraphDescriptorModel from './GraphDescriptorModel'

export default class GraphConstituentVnfd extends GraphDescriptorModel {

	constructor(graph, props) {
		super(graph, DescriptorModelFactory.ConstituentVnfd, props);
	}

}
