The backend system is managed by CONFD YANG files. These files contain details about what inputs the backend systems accept.

We generate a domain specific language (DSL) in JSON to drive rendering a "model driven" editor in the composer.


One Step:

Accomplish the below steps in one script `./generate-model-meta-json.sh`.

If you need to run the steps manually for debugging, etc. Then do the following.


Multi Step:

To generate this DSL, we take the following steps:

1. Collect the latest .yang files from the backend code bases and put them into this directory.

2. Run `yang2json.sh`

Requires yangforge to be installed `npm install -g yangforge`

This is brittle and requires removing any dependencies that fail. Usually, mano-types.yang has typedef references that do not have the 'manotype' prefix.
For example, 'type param-type-value' should be 'type manotypes:param-type-value'.

3. Run `node confd2model.js`

Review output to make sure it looks good.

4. Run `node confd2model.js > ./model-meta.json`

5. Copy the contents of ./model-meta.json into /webapp/src/libraries/DescriptorModelMeta.json

Verify the composer details editor is working properly.

Note:

The build servers do not like our .yang files so we append .src to the file names.

The yang2json.sh script does this for you now so you don' have to do the following. However, make sure you do not commit
.yang files because it will break the build.

Before committing these yang files, run `./src-append.sh` to append .src to the .yang files.

Before running the above steps remove the .src with `./src-remove.sh`.
