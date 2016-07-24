/**
 * Created by onvelocity on 2/11/16.
 */
'use strict';

import DescriptorModelFactory from '../model/DescriptorModelFactory'
import GraphDescriptorModel from './GraphDescriptorModel'

export default class GraphForwardingGraph extends GraphDescriptorModel {

	constructor(graph, props) {
		super(graph, DescriptorModelFactory.ForwardingGraph, props);
	}

}
