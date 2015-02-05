function ok(expect, msg) {
  postMessage({type: "status", status: !!expect, msg: msg});
}


onmessage = function(event) {
  ok(!!event.data.bitmap, "Get an ImageBitmpa from main script.");
  postMessage({"bitmap" : event.data.bitmap});
};