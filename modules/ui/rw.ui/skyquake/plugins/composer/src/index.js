import { render } from 'react-dom';
import SkyquakeRouter from 'widgets/skyquake_container/skyquakeRouter.jsx';
const config = require('../config.json');

// let context = require.context('./', true, /^\.\/.*\.(js|jsx|json)$/);
let context = require.context('./', true, /^\.\/.*\.(js|jsx)$/);
let router = SkyquakeRouter(config, context);
let element = document.querySelector('#app');

render(router, element);



