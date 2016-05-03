var urMus = {};
urMus.chat = {};

urMus.chat.addUser = function() {
	var alias = document.getElementById("chat_add_alias");
	urMus.chat.username.push(alias.value);
	urMus.chat.externalID.push(-1);
	alias.value = "";
}


urMus.chat.xhr = function() {
    
	return new XMLHttpRequest();
    
}

urMus.chat.postMessage = function() {
	
	var message = "";
    
	if (urMus.chat.internalID > 0) {
        message = document.getElementById("chat_message").value;
        if (message.charAt(0) == ":") {
            
            message = message.substr(1);

            urMus.chat.pushCode(message);
            document.getElementById("chat_message").value = "";

            return;
        }
        
        message = urMus.chat.internalID + "|" + encodeURIComponent(message);
   //     urMus.chat.pushMessage(urMus.chat.username[0] + ": " + document.getElementById("chat_message").value);

	}
	var xhr = urMus.chat.xhr();
	xhr.open("POST", "http://" + urMus.chat.IPAddress + ":8080/upload_script?file=" + urMus.chat.username[0] + ".chat.lua&contents=" + message, true);
	xhr.send(null);
	document.getElementById("chat_message").value = "";
	++urMus.chat.internalID;
    
}

urMus.chat.pushMessage = function(str) {
	
	var xhr = urMus.chat.xhr();
	xhr.open("POST", "http://" + urMus.chat.IPAddress + ":8080/eval?code=updateChatLog(\"" + str +"\")", true);
	xhr.send(null);
    
}

urMus.chat.pushCode = function(str) {
	
	var xhr = urMus.chat.xhr();
	xhr.open("POST", "http://" + urMus.chat.IPAddress + ":8080/eval?code=updateRunFunction('" + str +"', '"+urMus.chat.username[0]+"')", true);
	xhr.send(null);
    
}


urMus.chat.updateLog = function() {
	
	var xhr = new Array(urMus.chat.username.length);
    
	for (var i = 0; i < urMus.chat.username.length; ++i) {
        
		xhr[i] = urMus.chat.xhr();
        
		xhr[i].open("GET", "http://" + urMus.chat.IPAddress + ":8080/open_file?dirtype=doc&file=" + urMus.chat.username[i] + ".chat.lua", true);
		
		xhr[i].onreadystatechange = (function(index) {
                                     
                                     return function() {
                                     
                                     if (this.readyState == 4) {
                                     
                                     if (this.responseText.split("|").length > 1) { //Valid .chat format OLOLZ
                                     
                                     var message = this.responseText.split("|");
                                     if (message[0] > urMus.chat.externalID[index]) {
                                     urMus.chat.externalID[index] = message[0];
                                     var log = document.getElementById("chat_log");
                                     log.innerHTML += "<br><span class=\"chat_user" + index + "\">" + urMus.chat.username[index] + ":</span> " + decodeURIComponent(message[1]);
                                     log.scrollTop = log.scrollHeight;
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

urMus.chat.init = function(alias) {
   // alert("chat.js it's running")

	//Error check the alias in the future
    
//	document.getElementById("chat_post_init").style.display = "block";
//	document.getElementById("chat_pre_init").style.display = "none";
    
	urMus.chat.username = new Array();
	urMus.chat.externalID = new Array();
	urMus.chat.internalID = 0;
    
	urMus.chat.username.push(alias);
    
	urMus.chat.externalID.push(-1);
	urMus.chat.IPAddress = location.hostname;
    
	//Clear chat log for user
	urMus.chat.postMessage();
    
	document.getElementById("chat_message_submit").onclick = urMus.chat.postMessage;
	document.getElementById("chat_alias_submit").onclick = urMus.chat.addUser;
    
	setInterval(urMus.chat.updateLog, 1000);
    
}

//document.getElementById("chat_enter").onclick = urMus.chat.init;
