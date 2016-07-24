/**
 * Created by onvelocity on 2/11/16.
 */
'use strict';
import DescriptorModelFactory from '../model/DescriptorModelFactory'
import GraphDescriptorModel from './GraphDescriptorModel'

export default class GraphVirtualNetworkFunction extends GraphDescriptorModel {

	constructor(graph, props) {
		super(graph, DescriptorModelFactory.VirtualNetworkFunction, props);
	}

}
