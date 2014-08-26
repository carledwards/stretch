var initialized = false;

Pebble.addEventListener("ready", function() {
  //console.log("ready called!");
  initialized = true;
});

Pebble.addEventListener("showConfiguration", function() {
	var url = 'http://bengalbot.com/external/pebble/watchfaces/stretch/configuration.html?v=1.0';
	
  for(var i = 0, x = localStorage.length; i < x; i++) {
		var key = localStorage.key(i);
		var val = localStorage.getItem(key);
		
		if(val !== null) {
      url += "&" + encodeURIComponent(key) + "=" + encodeURIComponent(val);
		}
	}
	
  //console.log("showing configuration: " + url);
	Pebble.openURL(url);
});

Pebble.addEventListener("appmessage", function(e) {
    //console.log("Received message: " + e.payload);
    for(var key in e.payload) {
      localStorage.setItem(key, e.payload[key] == 1 ? "true" : "false");
    }
});

Pebble.addEventListener("webviewclosed", function(e) {
  //console.log("configuration closed");
  // webview closed
  var options = JSON.parse(decodeURIComponent(e.response));
  //console.log("Options = " + JSON.stringify(options));
  for(var key in options) {
    options[key] = options[key] ? 1 : 0
    localStorage.setItem(key, options[key]);
  }

  Pebble.sendAppMessage(options);
});
