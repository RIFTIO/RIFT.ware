import { render } from 'react-dom';
import SkyquakeRouter from 'widgets/skyquake_container/skyquakeRouter.jsx';
const config = require('json!../config.json');

let context = require.context('./', true, /^\.\/.*\.jsx$/);
let router = SkyquakeRouter(config, context);
let element = document.querySelector('#app');

render(router, element);


