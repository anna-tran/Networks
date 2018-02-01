# Sloxy

This program acts as a slow proxy ("sloxy" for short) between a web client and web server. It works using TCP, and slows down HTTP GET requests for non-cached HTML pages. The proxy first intercepts a HTTP request from a web page and parses the request. Using the request information, it sends a HEAD request to the server to predetermine what kind of HTTP response to handle. From there, there are 2 possible outcomes:
	1. The request is an HTTP GET to an HTML page and the HTTP response's status code is 200 OK
		If the HTML page is cached then the proxy simply sends a request to the server for the cached page, receives the page and sends it back to the client. If the HTML page is not cached, then the proxy verifies that the server can take range requests and submits multiple range requests (of 30 bytes per request). All the server response bodies are assembled and the proxy returns a 200 OK HTTP response to the client.
	2. The request and/or response is anything else
		In this case, the proxy simply prints out the status code and status phrase of the HTTP HEAD response and sends the response directly back to the client

## Usage

The "sloxy" will slow down the rate at which HTTP GET requests for HTML pages are delivered to the browser configured to send data to the proxy's port. The data is fetched from the web server at a fixed number of bytes per request. If the HTML page was cached and as a result, the HTTP response is a "304 Not Modified", then the proxy simply returns the cached page to the web client. This proxy uses blocking commands so it is only applicable to simple pages making synchronous requests to the web server.

## Compilation

To compile the program "sloxy.cpp",
	1. Run
			g++ sloxy.cpp -o sloxy
	2. Run
			./sloxy

## Configuration

To make sure the proxy is intercepting the web client requests and the web server responses, configure the browser the "sloxy" will be tested on to use a Web Proxy (HTTP). The host is 'localhost' for the user's computer or another IP on the same network. The default port is 4545 but if the user wishes to define his/her own port,
	1. Choose a port number (1024-65535) for the proxy 
	2. Change the port in the "sloxy.cpp" before compilation to the chosen port

It may be useful to clear the browser's cache in case the pages to be tested using the proxy have been cached.
By default, the byte rate at which the proxy requests is 30 bytes per request. This can also be changed in the "sloxy.cpp" before compilation.

## Testing

The "sloxy" was tested on this local computer, which runs OS X. The test example TXT and HTML pages on http://pages.cpsc.ucalgary.ca/~carey/CPSC441/assignment1.html were used for testing, as well as other simple HTML pages found online.