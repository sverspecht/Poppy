#!/bin/bash
function red { echo -ne "\033[1;31m$1\033[0m"; tput sgr0; }

set -e 
#rm -r web/
mkdir -p web

echo "The content of this directory is generated by the makeweb.sh script. All changes in this directory might get lost!" > web/README
SEDSTR='s#poppy.worker.js#./get.php?res=poppy.worker.js#g;s#poppy.wasm#./get.php?res=poppy.wasm#g;'
make clean;
AUTOVECTOR=1 WASM=1 make -j8; sed -i "$SEDSTR" src/poppy.js; cp third/webm-wasm-0.4.1/dist/webm-wasm.* src/poppy.data src/poppy.html src/poppy.js src/poppy.wasm src/poppy.worker.js web/

rm -f poppyweb.zip
cd web/
zip -r ../poppyweb.zip *

red "Success\n"

