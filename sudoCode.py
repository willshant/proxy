receive request;
GET
if (map.find(url)){
	if (response.no_cache == true || response.expiration_time < time(0)){
		if (response.ETAG != ""){
			generate new request with "If-None-Match: <ETAG>";
			send to original server;
		}
		else if (response.Last_modified != ""){
			generate new request with "If-Modified-Since: <http time>";
			send to original server;
		}
		else {
			send original request to original server;
		}
	}
	else {
		send response in cache to client;
	}
}
else {
	send original request to original server;
}

receive response;

if (response.symbol == 304){
	send response in cache to client;
}
else if(response.symbol == 200){
	send new response to client;
	if (response.if_cache == true){
		map.insert(response);
	}
}
else {
	send corresponding response to client;
}