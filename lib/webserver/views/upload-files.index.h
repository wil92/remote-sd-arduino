
#ifndef UPLOAD_FILES_INDEX_HTML_H
#define UPLOAD_FILES_INDEX_HTML_H

const char upload_files_index_html[] PROGMEM = R"PAGE(
<!doctype html><title>Upload files</title><style>ul{padding:0;list-style-type:none}li{margin:5px 0}button{margin-left:10px}</style><body><h1>Upload files page</h1><h2>Files on the SD</h2><input id=file_input type=file><button id=upload_button>upload file</button><ul>$$FILES$$ </ul></body><script>const but=document.getElementById(`upload_button`);but.onclick=function(){let file=document.getElementById(`file_input`).files[0];if(!file){console.error(`No file selected`);return}let formData=new FormData;formData.append(`file`,file),fetch(`/upload`,{method:`POST`,body:formData}).then(res=>{console.log(res),location.reload()})};function del(fileName){fetch(`/?file=${encodeURIComponent(fileName)}`,{method:`DELETE`}).then(res=>{console.log(res),location.reload()}).catch(e=>console.error(e))}</script>
)PAGE";

#endif //UPLOAD_FILES_INDEX_HTML_H
