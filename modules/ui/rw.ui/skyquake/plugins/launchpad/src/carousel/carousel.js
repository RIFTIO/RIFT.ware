
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
require('./carousel.html');
require('./carousel-react.jsx')
var Slider = require('react-slick');
angular.module('carousel')
  .directive('fleetCard', function($state, $stateParams) {
    return {
      restrict: 'AE',
      replace: true,
      templateUrl: '/modules/launchpad/carousel/carousel.html',
      scope: {
      },
      link: function() {

      },
      bindToController: true,
      controllerAs: 'card',
      controller: function($timeout, $interval, $scope) {



      }
    };
  })
