
The application enables editing of CONFD YANG instances.

Catalog Panel - loads the NSD and VNFD and PNFD catalogs from the server and updates the internal indexes used throughout
	the UI
Canvas Panel - graphical editor of the relations and networks of NSD and VNFD descriptors
Details Panel - schema driven editor of every property in the YANG models
Forwarding Graphs Tray - editing FG RSP, Classifier and MatchAttribute properties

# Details Panel

 - To get an object to show up in the Details Panel it must be defined in the DescriptorModelMeta.json schema file.

 - only needs the DescriptorModelMeta.json file to define the JSON to create / edited.

 - To make an object appear in the Details Panel you need to add it to the "containersList" in the DescriptorModelFactor.js class.

# Canvas Panel

 - is coded specifically to enable graphical editing of certain descriptor model elements and is the least flexible

 - To make an object "selectable" it must have a data-uid field.

 The canvas panel uses D3 to render the graphical editing experience.

# State Management

There are two layers of state: 1) model state, 2) UI state.

The YANG models are wrapped in Class objects to facilitate contextual operations that may change either state, like
adding and removing property values, accessing the underlying uiState object, etc. These classes are defined in the
`src/libraries/model/` directory.

## Model State

The UI uses Alt.js implementation of Flux. Alt.js provides for the actions and state of the application. Model state is
managed by the CatalogDataStore. Any change made to the model must notify the CatalogDataStore. Upon notification of a
change the Alt DataStore will setState() with a deep clone of the model causing a UI update.

You will see `CatalogItemsActions.catalogItemDescriptorChanged(catalogItemModel)` everywhere a change is made to the
model. In essence the UI treats the model as immutable. While the object is technically mutable the UI is modifying a copy
of the model and so for changes to 'stick' the UI must notify the CatalogDataStore.

## UI State

UI state is managed in a couple different ways depending on the specific need of the UI. The three ways are: 1) a field
named 'uiState' added to each YANG model instance when the catalog is loaded from the server; 2) React Component state not
saved in the Alt DataStore; and 3) module variables.

Ideally, all uiState would us the later two methods. The 'uiState' field poses a potential name collision with the YANG
model (not likely, but if it happens could be really bad for the application!).

## ReactJS and d3

The components built using d3 own the management of the DOM under the SVGSVGElement. ReactJS manages the content DOM element
above the SVG element. This is a clean separation of concerns. Any model or UI state changes are handled by the model
classes and therefore d3 is agnostic and ignorant of managing state. ReactJS is not responsible for the DOM below the
SVG content DIV and so does not care about any of the DOM manipulations that d3 makes.

All of the UI is driven by the model which is always passed down through the props of the parent ReactJS Component. The
d3 components provide a way to pass the model and UI state into them. For an example of this look at the
`CatalogItemCanvasEditor::componentDidMount()` method. You will see the parent content div and the model data are given
to the `DescriptorGraph()` d3 component.

The d3 graphing components are located in the `src/libraries/graph/` directory.




