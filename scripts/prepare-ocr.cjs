const fs=require("node:fs"),path=require("node:path");
const root=path.resolve(__dirname,".."),dest=path.join(root,"ocr"),modules=path.join(root,"node_modules");
function copy(src,target){fs.mkdirSync(path.dirname(target),{recursive:true});fs.copyFileSync(src,target);}
for(const file of ["tesseract.min.js","worker.min.js"])copy(path.join(modules,"tesseract.js","dist",file),path.join(dest,file));
const core=path.join(modules,"tesseract.js-core");
for(const file of fs.readdirSync(core).filter(f=>/\.wasm(?:\.js)?$/.test(f)))copy(path.join(core,file),path.join(dest,"core",file));
copy(path.join(modules,"@tesseract.js-data","eng","4.0.0_best_int","eng.traineddata.gz"),path.join(dest,"lang","eng.traineddata.gz"));
console.log("OCR preparado.");
