/*
 var urMus = {};
urMus.chat = {};

urMus.chat.addUser = function() {
	
	urMus.chat.username.push(document.getElementById("post_chat_name").value);
	urMus.chat.IPAddress.push(document.getElementById("post_chat_ip").value);
	urMus.chat.externalID.push(-1);

}


urMus.chat.xhr = function() {

	return new XMLHttpRequest();

}

urMus.chat.postMessage = function() {
	var message = urMus.chat.internalID + "|" + encodeURIComponent(document.getElementById("post_chat").value);
	var xhr = urMus.chat.xhr();
	xhr.open("POST", "http://" + urMus.chat.IPAddress[0] + ":8080/upload_script?file=" + urMus.chat.IPAddress[0] + ".chat&contents=" + message, true);
	xhr.send(null);
	document.getElementById("post_chat").value = "";
	++urMus.chat.internalID;

}

urMus.chat.updateLog = function() {
	
	var xhr = new Array(urMus.chat.IPAddress.length);

	for (var i = 0; i < urMus.chat.IPAddress.length; ++i) {
	
		xhr[i] = urMus.chat.xhr();

		xhr[i].open("GET", "http://" + urMus.chat.IPAddress[i] + ":8080/open_file?dirtype=doc&file=" + urMus.chat.IPAddress[i] + ".chat", true);
		
		xhr[i].onreadystatechange = (function(index) { 
		
			return function() {

				if (this.readyState == 4) {

					if (this.responseText.split("'")[0] != "Can") { //Can't open file, i.e. doesn't exist

						var message = this.responseText.split("|");
						if (message[0] > urMus.chat.externalID[index]) {
							urMus.chat.externalID[index] = message[0];
							document.getElementById("chat_log").innerHTML += "<br>" + urMus.chat.username[index] + ": " + decodeURIComponent(message[1]);
						}

						if (urMus.chat.internalID <= urMus.chat.externalID[index]) {
							urMus.chat.internalID = urMus.chat.externalID[index] + 1;
						}
					}
				}
			};

		})(i);

		xhr[i].send();
	}

}

urMus.chat.init = function() {

    alert("it's running")
	urMus.chat.username = new Array();
	urMus.chat.IPAddress = new Array();
	urMus.chat.externalID = new Array();
	urMus.chat.internalID = 0;

	urMus.chat.username.push("Me");
	urMus.chat.externalID.push(-1);
	urMus.chat.IPAddress.push(location.hostname);

	document.getElementById("post_chat_button").onclick = urMus.chat.postMessage;
	document.getElementById("post_chat_submit").onclick = urMus.chat.addUser;

	setInterval(urMus.chat.updateLog, 1000);

}

urMus.chat.init();
*/
