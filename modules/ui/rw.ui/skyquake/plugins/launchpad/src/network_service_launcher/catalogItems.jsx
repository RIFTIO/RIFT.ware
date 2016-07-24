
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */
import React, {Component} from 'react';
import './catalog_items.scss';
import Loader from 'widgets/loading-indicator/loadingIndicator.jsx';

import nsdImg from 'style/img/catalog-default.svg';

export default class CatalogItems extends Component {
  constructor(props) {
    super(props);
  }

  render() {
    let self = this;
    let classNames = 'catalogItems';
    let outerSpanClassNames = 'outerDefault';
    let catalogItems = [];
    self.props.catalogs.forEach((catalog) => {
      catalog.descriptors && catalog.descriptors.map((item, i) => {
        let itemClassName = classNames + '_item';
        if (self.props.selectedNSDid == item.id) {
          itemClassName += ' -is-selected';
        }
        let vendorHTML = (item.vendor || item.provider) + ' / ' + 'v' + item.version;
        catalogItems.push((
          <li key={'id' + i + '-' + item.id} className={itemClassName} onClick={this.props.onSelect.bind(this, item)}>
            <img src={nsdImg} />
            <section id={item.id}>
              <h2>{item.name}</h2>
              <abbr className="code-name" title={item.name}>{item['short-name']}</abbr>
              <span className="vendor">{vendorHTML}</span>
            </section>
          </li>
        ))
      })
    })

    if (!self.props.catalogs.length) {
      catalogItems.push((<li key="loader" style={{textAlign: 'center'}}><Loader show={true} /></li>))
    }
    return (
      <div>
        <ul className={classNames}>
          {catalogItems}
        </ul>
      </div>
    );
  }
}

CatalogItems.defaultProps =  {
    catalogs: [],
    onSelect: function(item) {
      console.log(item,  ' clicked');
      this.setState({
        selectedItem: item
      });
    }
}
