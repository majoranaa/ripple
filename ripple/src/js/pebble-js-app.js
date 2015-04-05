var num_samples = 20;
var i = 0;
var data = [];
var ip = '128.97.179.236';

function xhrRequest(url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
}

/*
function parseAccelData(payload) {
    //console.log(JSON.stringify(payload,null,2));
    var accel_data = payload['KEY_DATA'];
    for (i = 0; i < num_samples; i++) {
	data[i] = {
	    x: (accel_data[i]<<8 + (accel_data[i+1])), // 65536 = 1<<16
	    y: (accel_data[i+2] + (accel_data[i+3]<<8)) - 65536,
	    z: (accel_data[i+4] + (accel_data[i+5]<<8)) - 65536,
	    did_vibrate: accel_data[i+6],
	    timestamp: accel_data[i+7] + (accel_data[i+8]<<8) + (accel_data[i+9]<<16) + (accel_data[i+10]<<24) + (accel_data[i+11]<<32) + (accel_data[i+12]<<40) + (accel_data[i+13]<<48) + (accel_data[i+14]<<56)
	};
    }
    console.log(JSON.stringify(data,null,2));
}
*/

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
			function(e) {
			    console.log('PebbleKit JS ready!');
			}
		       );

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
			function(e) {
			    console.log('AppMessage received!');
			    //console.log(JSON.stringify(e,null,2));
			    //parseAccelData(e.payload);
			    xhrRequest('http://'+ip+'/ripple/?data='+JSON.stringify(e.payload['KEY_DATA']), 'GET', function(response) {
			    });
			});
