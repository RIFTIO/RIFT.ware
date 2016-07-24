/**
 * Created by onvelocity on 2/11/16.
 */
'use strict';

import d3 from 'd3'
import ClassNames from 'classnames'
import ColorGroups from '../ColorGroups'
import SelectionManager from '../SelectionManager.js'
import DescriptorModelFactory from '../model/DescriptorModelFactory'
import DescriptorGraphSelection from './DescriptorGraphSelection'
import CatalogItemsActions from '../../actions/CatalogItemsActions'
import GraphDescriptorModel from './GraphDescriptorModel'

import '../../styles/GraphVirtualLink.scss'

export default class GraphVirtualLink extends GraphDescriptorModel {

	constructor(graph, props) {
		super(graph, DescriptorModelFactory.VirtualLink, props);
	}

}
