/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
/**
 * Created by onvelocity on 10/6/15.
 *
 * Model fragments used to construct partially completed models.
 */

'use strict';

import guid from './../guid'
import InstanceCounter from './../InstanceCounter'

export default {
	'vnfd': {
		'id': '5b9af24e-2c8f-4792-9d6e-ff9eabb97f15',
		'name': 'vnfd-1',
		'short-name': 'vnfd-1',
		'description': 'A simple VNF descriptor w/ one VDU',
		'version': '1.0',
		'connection-point': [
			{
				'name': 'cp1',
				'type': 'VPORT'
			}
		],
		'vdu': [
			{
				'id': 'abd6831e-f811-4580-9aad-1de9c6424180',
				'name': 'vdu-1',
				'vm-flavor': {
					'vcpu-count': 4,
					'memory-mb': 16384,
					'storage-gb': 16
				},
				'image': '',
				'external-interface': [
					{
						'name': 'eth0',
						'vnfd-connection-point-ref': 'cp1',
						'virtual-interface': {
							'type': 'VIRTIO'
						}
					}
				]
			}
		]
	},
	'vnfd.internal-vld': {
		'id': () => guid(),
		'name': () => 'vld-' + InstanceCounter.count('new.vnfd.internal-vld'),
		'description': 'Virtual link for internal fabric',
		'type': 'ELAN'
	}
};
