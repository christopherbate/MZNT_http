from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
import threading
import subprocess
import os
import re
import paho.mqtt.client as mqtt

# Callback when disconnected from MQTT Broker
def on_disconnect( client, userdata,rc ):
    if( rc != 0):
        print("HTTP Server disconnected unexpectedly from MQTT")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    # Pass message to the LeaderController
    print("HTTP Server received message.")

def on_connect(client, userdata, msg, rc):
    print("Connected on MQTT")

client = mqtt.Client()
client.username_pw_set("mznt", "rj39SZSz")
client.on_connect = on_connect
client.on_message = on_message
client.connect( "gnssfast.colorado.edu" , 5554, 60)
client.loop_start()

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
            print("Got resource for path: " + str(path))
            length = int(self.headers['Content-Length'])
            print("Length is: " + str(length))

            os.makedirs(os.path.dirname(path), exist_ok=True)
            print("Attempting to write file.")
            with open(path, 'wb') as f:
                f.write(self.rfile.read(length))
            print("Done")

            self.protocol_version = 'HTTP/1.1'
            self.send_response(201,"created") #create header
            self.send_header("Content-Length", 0)
            self.end_headers()

            client.publish( "files", path, 0 )

class ThreadedHTTPServer(HTTPServer):
    """Handle requests in a separate thread."""

if __name__ == "__main__":
    server = ThreadedHTTPServer(('', 1337), Handler)
    print('Starting server, use <Ctrl-C> to stop')
    server.serve_forever()
    client.loop_stop()
