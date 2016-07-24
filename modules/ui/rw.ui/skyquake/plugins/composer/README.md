RIFT.io UI
===
Currently this repo only contains one module.

# Development Setup

## Requirements

```
npm install -g babel
npm install -g grunt
```

## Helpful

```
npm install -g yo
npm install -g generator-react-webpack # https://github.com/newtriks/generator-react-webpack
```

# Build Steps

```
npm install
grunt build:dist # production build
grunt build      # dev build
grunt test 	     # run tests
```

# Development Steps

```
grunt serve      # start webpack dev server source-maps
grunt serve:dist # start dev server with dist runtime
```

## Known Issues
`grunt serve:dist` fails for unknown reason. workaround use python -m SimpleHTTPServer 8099

# Useful Libs

• [http://numeraljs.com/](http://numeraljs.com)

• [http://momentjs.com/docs/](http://momentjs.com/docs/)

# How the code works see ./src/README.md
