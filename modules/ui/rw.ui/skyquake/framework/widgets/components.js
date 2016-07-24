
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */

var React = require('react');
//var Histogram = require('react-d3-histogram')
export default {
  //test: require('./test/test.js'),
  button: require('./button/rw.button.js'),
  React: React,
//  Histogram: Histogram,
  Multicomponent: require('./multicomponent/multicomponent.js'),
  Mixins: require('./mixins/ButtonEventListener.js'),
  Gauge: require('./gauge/gauge.js'),
  Bullet: require('./bullet/bullet.js')
};

// require('../../assets/js/n3-line-chart.js');
// var Gauge = require('../../assets/js/gauge-modified.js');
// var bulletController = function($scope, $element) {
//     this.$element = $element;
//     this.vertical = false;
//     this.value = 0;
//     this.min = 0;
//     this.max = 100;
//     //this.range = this.max - this.min;
//     //this.percent = (this.value - this.min) / this.range;
//     this.displayValue = this.value;
//     this.isPercent = (this.units == '')? true:false;
//     this.bulletColor = "#6BB814";
//     this.fontsize = 28;
//     this.radius = 4;
//     this.containerMarginX = 0;
//     this.containerMarginY = 0;
//     this.textMarginX = 5;
//     this.textMarginY = 42;
//     this.bulletMargin = 0;
//     this.width = 512;
//     this.height = 64;
//     this.markerX = -100; // puts it off screen unless set
//     var self = this;
//     if (this.isPercent) {
//         this.displayValue + "%";
//     }
//     $scope.$watch(
//       function() {
//         return self.value;
//       },
//       function() {
//         self.valueChanged();
//       }
//     );

//   }

//   bulletController.prototype = {

//     valueChanged: function() {
//       var range = this.max - this.min;
//       var normalizedValue = (this.value - this.min) / range;
//       if (this.isPercent) {
//         this.displayValue = String(Math.round(normalizedValue * 100)) + "%";
//       } else {
//         this.displayValue = this.value;
//       }
//       // All versions of IE as of Jan 2015 does not support inline CSS transforms on SVG
//       if (platform.name == 'IE') {
//         this.bulletWidth = Math.round(100 * normalizedValue) + '%';
//       } else {
//         this.bulletWidth = this.width - (2 * this.containerMarginX);
//         var transform = 'scaleX(' + normalizedValue + ')';
//         var bullet = $(this.$element).find('.bullet2');
//         bullet.css('transform', transform);
//         bullet.css('-webkit-transform', transform);
//       }
//     },

//     markerChanged: function() {
//       var range = this.max - this.min;
//       var w = this.width - (2 * this.containerMarginX);
//       this.markerX = this.containerMarginX + ((this.marker - this.min) / range ) * w;
//       this.markerY1 = 7;
//       this.markerY2 = this.width - 7;
//     }
//   }

// angular.module('components', ['n3-line-chart'])
//     .directive('rwBullet', function() {
//       return {
//         restrict : 'E',
//         templateUrl: 'modules/views/rw.bullet.tmpl.html',
//         bindToController: true,
//         controllerAs: 'bullet',
//         controller: bulletController,
//         replace: true,
//         scope: {
//           min : '@?',
//           max : '@?',
//           value : '@',
//           marker: '@?',
//           units: '@?',
//           bulletColor: '@?',
//           label: '@?'
//         }
//       };
//     })
//     .directive('rwSlider', function() {
//       var controller = function($scope, $element, $timeout) {
//         // Q: is there a way to force attributes to be ints?
//         $scope.min = $scope.min || "0";
//         $scope.max = $scope.max || "100";
//         $scope.step = $scope.step || "1";
//         $scope.height = $scope.height || "30";
//         $scope.orientation = $scope.orientation || 'horizontal';
//         $scope.tooltipInvert = $scope.tooltipInvert || false;
//         $scope.percent = $scope.percent || false;
//         $scope.kvalue = $scope.kvalue || false;
//         $scope.direction = $scope.direction || "ltr";
//         $($element).noUiSlider({
//           start: parseInt($scope.value),
//           step: parseInt($scope.step),
//           orientation: $scope.orientation,
//           range: {
//             min: parseInt($scope.min),
//             max: parseInt($scope.max)
//           },
//           direction: $scope.direction
//         });
//         //$(".no-Ui-target").Link('upper').to('-inline-<div class="tooltip"></div>')
//         var onSlide = function(e, value) {
//           $timeout(function(){
//             $scope.value = value;
//           })

//         };
//         $($element).on({
//           change: onSlide,
//           slide: onSlide,
//           set: $scope.onSet({value: $scope.value})
//         });
//         var val = String(Math.round($scope.value));
//         if ($scope.percent) {
//             val += "%"
//         } else if ($scope.kvalue) {
//             val += "k"
//         }
//         $($element).height($scope.height);
//         if ($scope.tooltipInvert) {
//             $($element).find('.noUi-handle').append("<div class='tooltip' style='position:relative;right:20px'>" + val + "</div>");
//         } else {
//             $($element).find('.noUi-handle').append("<div class='tooltip' style='position:relative;left:-20px'>" + val + "</div>");
//         }
//         $scope.$watch('value', function(value) {
//         var val = String(Math.round($scope.value));
//         if ($scope.percent) {
//             val += "%"
//         } else if($scope.kvalue) {
//             val += "k"
//         }
//           $($element).val(value);
//           $($element).find('.tooltip').html(val);
//         if ($scope.tooltipInvert) {
//             $($element).find('.tooltip').css('right', $($element).find('.tooltip').innerWidth() * -1);
//         } else {
//             $($element).find('.tooltip').css('left', $($element).find('.tooltip').innerWidth() * -1);
//         }
//         });
//       };

//       return {
//         restrict : 'E',
//         template: '<div></div>',
//         controller : controller,
//         replace: true,
//         scope: {
//           min : '@',
//           max : '@',
//           width: '@',
//           height: '@',
//           step : '@',
//           orientation : '@',
//           tooltipInvert: '@',
//           percent: '@',
//           kvalue: '@?',
//           onSet:'&?',
//           direction: '@?',
//           value:'=?'
//         }
//       };
//     })
// .directive('rwGauge', function() {
//     return {
//         restrict: 'AE',
//         template: '<canvas class="rwgauge" style="width:100%;height:100%;max-width:{{width}}px;max-height:240px;"></canvas>',
//         replace: true,
//         scope: {
//             min: '@?',
//             max: '@?',
//             size: '@?',
//             color: '@?',
//             value: '@?',
//             resize: '@?',
//             isAggregate: '@?',
//             units: '@?',
//             valueFormat: '=?',
//             width: '@?'
//         },
//         bindToController: true,
//         controllerAs: 'gauge',
//         controller: function($scope, $element) {
//             var self = this;
//             this.gauge = null;
//             this.min = this.min || 0;
//             this.max = this.max || 100;
//             this.nSteps = 14;
//             this.size = this.size || 300;
//             this.units = this.units || '';
//             $scope.width = this.width || 240;
//             this.color = this.color || 'hsla(212, 57%, 50%, 1)';
//             if (!this.valueFormat) {
//                 if (this.max > 1000 || this.value) {
//                     self.valueFormat = {
//                         "int": 1,
//                         "dec": 0
//                     };
//                 } else {
//                     self.valueFormat = {
//                         "int": 1,
//                         "dec": 2
//                     };
//                 }
//             }
//             this.isAggregate = this.isAggregate || false;
//             this.resize = this.resize || false;
//             if (this.format == 'percent') {
//                 self.valueFormat = {
//                     "int": 3,
//                     "dec": 0
//                 };
//             }
//             $scope.$watch(function() {
//                 return self.max;
//             }, function(n, o) {
//                 if(n !== o) {
//                     renderGauge();
//                 }
//             });
//             $scope.$watch(function() {
//                 return self.valueFormat;
//             }, function(n, o) {
//                 if(n != 0) {
//                     renderGauge();
//                 }
//             });
//             $scope.$watch(function() {
//                 return self.value;
//             }, function() {
//                 if (self.gauge) {
//                     // w/o rounding gauge will unexplainably thrash round.
//                     self.valueFormat = determineValueFormat(self.value);
//                     self.gauge.setValue(Math.ceil(self.value * 100) / 100);
//                     //self.gauge.setValue(Math.round(self.value));
//                 }
//             });
//             angular.element($element).ready(function() {
//                 console.log('rendering')
//                 renderGauge();
//             })
//             window.testme = renderGauge;
//             function determineValueFormat(value) {

//                     if (value > 999 || self.units == "%") {
//                         return {
//                             "int": 1,
//                             "dec": 0
//                         }
//                     }

//                     return {
//                         "int": 1,
//                         "dec": 2
//                     }
//                 }
//             function renderGauge(calcWidth) {
//                 if (self.max == self.min) {
//                     self.max = 14;
//                 }
//                 var range = self.max - self.min;
//                 var step = Math.round(range / self.nSteps);
//                 var majorTicks = [];
//                 for (var i = 0; i <= self.nSteps; i++) {
//                     majorTicks.push(self.min + (i * step));
//                 };
//                 var redLine = self.min + (range * 0.9);
//                 var config = {
//                     isAggregate: self.isAggregate,
//                     renderTo: angular.element($element)[0],
//                     width: calcWidth || self.size,
//                     height: calcWidth || self.size,
//                     glow: false,
//                     units: self.units,
//                     title: false,
//                     minValue: self.min,
//                     maxValue: self.max,
//                     majorTicks: majorTicks,
//                     valueFormat: determineValueFormat(self.value),
//                     minorTicks: 0,
//                     strokeTicks: false,
//                     highlights: [],
//                     colors: {
//                         plate: 'rgba(0,0,0,0)',
//                         majorTicks: 'rgba(15, 123, 182, .84)',
//                         minorTicks: '#ccc',
//                         title: 'rgba(50,50,50,100)',
//                         units: 'rgba(50,50,50,100)',
//                         numbers: '#fff',
//                         needle: {
//                             start: 'rgba(255, 255, 255, 1)',
//                             end: 'rgba(255, 255, 255, 1)'
//                         }
//                     }
//                 };
//                 var min = config.minValue;
//                 var max = config.maxValue;
//                 var N = 1000;
//                 var increment = (max - min) / N;
//                 for (i = 0; i < N; i++) {
//                     var temp_color = 'rgb(0, 172, 238)';
//                     if (i > 0.5714 * N && i <= 0.6428 * N) {
//                         temp_color = 'rgb(0,157,217)';
//                     } else if (i >= 0.6428 * N && i < 0.7142 * N) {
//                         temp_color = 'rgb(0,142,196)';
//                     } else if (i >= 0.7142 * N && i < 0.7857 * N) {
//                         temp_color = 'rgb(0,126,175)';
//                     } else if (i >= 0.7857 * N && i < 0.8571 * N) {
//                         temp_color = 'rgb(0,122,154)';
//                     } else if (i >= 0.8571 * N && i < 0.9285 * N) {
//                         temp_color = 'rgb(0,96,133)';
//                     } else if (i >= 0.9285 * N) {
//                         temp_color = 'rgb(0,80,112)';
//                     }
//                     config.highlights.push({
//                         from: i * increment,
//                         to: increment * (i + 2),
//                         color: temp_color
//                     })
//                 }
//                 var updateSize = _.debounce(function() {
//                     config.maxValue = self.max;
//                     var clientWidth = self.parentNode.parentNode.clientWidth / 2;
//                     var calcWidth = (300 > clientWidth) ? clientWidth : 300;
//                     self.gauge.config.width = self.gauge.config.height = calcWidth;
//                     self.renderGauge(calcWidth);
//                 }, 500);
//                 if (self.resize) $(window).resize(updateSize)
//                 if (self.gauge) {
//                     self.gauge.updateConfig(config);
//                 } else {
//                     self.gauge = new Gauge(config);
//                     self.gauge.draw();
//                 }
//             };
//         },
//     }
// });
