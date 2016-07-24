
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React from 'react';
import RWslider from 'widgets/input-range-slider/input-range-slider.jsx';
import {Tab, Tabs, TabList, TabPanel} from 'react-tabs';
import CardActions from './launchpadCardActions.js';

let currentID, lastSelected;

function isTabNode(node) {
  return node.nodeName === 'LI' && node.getAttribute('role') === 'tab';
}
class launchpadControls extends React.Component {
  constructor(props, context) {
    super(props, context);
    this.state = {
      controlSets:  this.props.controlSets,
      selectedIndex: 0,
      currentId: 0
    };
    this.handleInputUpdate = this.handleInputUpdate.bind(this);
    this.handleSelected = this.handleSelected.bind(this);
    this.handleSameTab = this.handleSameTab.bind(this);
  }
  handleSelected(index,last) {
      this.state.selectedIndex = index;
  }
  //If same Tab is clicked, default to empty tab contents
  handleSameTab(e) {
    var node = e.target.parentNode;
    var isTab = isTabNode(node);
    if(isTab) {
       if(node.id == currentID) {
        this.state.selectedIndex = 0;
        currentID = null;
        this.forceUpdate();
      } else {
        currentID = node.id
      }
    }
  }
  componentWillReceiveProps(nextProps) {
    if((!this.state.controlSets && nextProps.controlSets) || this.state.controlSets && !nextProps.controlSets) {
      this.state.controlSets = nextProps.controlSets;
    };

  }
  shouldComponentUpdate(nextProps, nextState) {
    // console.log(this.props, nextProps);
    // console.log(this.state, nextState)
    return true;
  }
  handleInputUpdate(gIndex, groupID, cIndex, options, value) {
    this.state.controlSets[gIndex][groupID]['control-param'][cIndex].value = value;
    CardActions.updateControlInput(options, value)
  }
  render() {
    let self = this;
    if(!this.state.controlSets) {
      return (<div></div>)
    } else {
      return (
        <div className="launchpadCard_controls" onClick={self.handleSameTab}>
        {
          this.state.controlSets.map(function(group, gIndex) {
            for(let id in group) {
              return (
                <Tabs key={gIndex} selectedIndex={self.state.selectedIndex}  onSelect={self.handleSelected}>

                  <TabList>
                    <Tab>
                      {
                        group[id]['action-param'].map(function(action, aIndex) {
                          return (
                            <button key={aIndex} className="light small" title={action.name}>{action.name}</button>
                          )
                        })
                      }
                    </Tab>
                    {
                      group[id]['control-param'].map(function(control, cIndex) {
                        return (
                          <Tab key={cIndex}>
                          <span  title={control.name}>
                            {control.name} : {control.value || control["current-value"]} {control.units}
                            </span>
                          </Tab>
                        )
                      })
                    }
                  </TabList>
                  <TabPanel actionplaceholder>
                  </TabPanel>
                  {
                    group[id]['control-param'].map(function(control, cIndex) {
                      return (
                        <TabPanel key={cIndex}>
                          <RWslider {...control} handleInputUpdate={self.handleInputUpdate.bind(self, gIndex, id, cIndex, {operation:control.operation,
                            url:control.url
                          })}/>
                        </TabPanel>
                      )
                    })
                  }
                </Tabs>
              )
            }
          })
        }
        </div>
      )
    }
  }
};

export default launchpadControls;


/**
 *
 *   grouping control-param {
    list control-param {
      description
          "List of control parameters to manage and
           update the running configuration of the VNF";
      key id;

      leaf id {
        type string;
      }

      leaf name {
        type string;
      }

      leaf description {
        type string;
      }

      leaf group-tag {
        description "A simple tag to group control parameters";
        type string;
      }

      leaf min-value {
        description
            "Minimum value for the parameter";
        type uint64;
      }

      leaf max-value {
        description
            "Maxium value for the parameter";
        type uint64;
      }

      leaf step-value {
        description
            "Step value for the parameter";
        type uint64;
      }

      leaf units {
        type string;
      }

      leaf widget-type {
        type manotypes:widget-type;
      }
    }
  }
 */
