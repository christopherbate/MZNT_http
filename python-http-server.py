from http.server import BaseHTTPRequestHandler, HTTPServer
import os

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self._set_headers()
        self.wfile.write("<html><body><h1>hi!</h1></body></html>")

    def do_POST(self):
        # Doesn't do anything with posted data
        self._set_headers()
        self.wfile.write("<html><body><h1>POST!</h1></body></html>")
        
    def do_PUT(self):
        path = self.path
        if path.endswith('/'):
            self.send_response(405, "Method Not Allowed")
            self.wfile.write("PUT not allowed on a directory\n".encode())
            return
        else:
            path = '.' + path
            length = int(self.headers['Content-Length'])
            
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as f:
                f.write(self.rfile.read(length))
            
            self.protocol_version = 'HTTP/1.1'
            #self.send_response(201, "Created") 
            #self.handle_expect_100()
            #response = bytes("This is the response.", "utf-8")
            self.send_response(200) #create header
            #self.send_header("Content-Length", str(len(response)))
            self.send_header("Content-Length", 0)
            self.end_headers()
            
            #self.wfile.write(response) #send response

def run(server_class=HTTPServer, handler_class=Handler, port=8888):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print('Starting httpd...')
    httpd.serve_forever()

if __name__ == "__main__":
    from sys import argv
    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
