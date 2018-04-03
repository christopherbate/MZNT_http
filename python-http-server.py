from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
import threading
import subprocess
import os
import re

class Handler(BaseHTTPRequestHandler):
    def file_handler(self,path):
        print("In file handler")
        print(path)
        name = re.split(r'[/]+', path)[-1]
        print(name)
        # Split name on both periods and underscores
        name_list = re.split(r'[._]+', name)
        if ('IF' not in name_list[1]) or (len(name_list) < 3):
            return

        # key is filename, without extension & pre/post
        key = name_list[0] + '_' + name_list[1] + '_' + name_list[2]
        
        print("Attempting to start postprocessing")
        # Dir name is node_data.*
        mz_dir = './'
        dir_name = key
        subpath = mz_dir + dir_name
        print(subpath)
        os.system('mkdir ' + subpath)

        # Move all files with this naming convention into new directory
        # mv should be atomic
        print('mv ' + mz_dir + name + ' ' + subpath + '/' + key)
        os.system('mv ' + path + ' ' + subpath + '/' + key)

        # cp scripts into new directory
        os.system('cp testing_pipeline.sh ' + subpath + '/test.sh')
        #os.system('cp concatfiles.sh ./' +  subpath + '/concatfiles.sh')
        os.system('cp unpacker ./' + subpath + '/unpacker')
        
        print("Calling shell script")
        # Code to call shell script with command line arguments
        subprocess.Popen('./test.sh ' + key + ' ' + 'unneeded' + ' ' + '/home/usrpdata/disk2/mznt_chris/GNSS_SDR_v1-93', shell=True, cwd=subpath)

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
                #data = self.rfile.read(10)
                #while(data):
                #    print(data)
                #    f.write(data)
                #    data = self.rfile.read(10)
                f.write(self.rfile.read(length))
            print("Done")
            #self.file_handler(path)

            self.protocol_version = 'HTTP/1.1'
            #self.send_response(201, "Created")
            #self.handle_expect_100()
            #response = bytes("This is the response.", "utf-8")
            self.send_response(200) #create header
            #self.send_header("Content-Length", str(len(response)))
            self.send_header("Content-Length", 0)
            self.end_headers()

    #self.wfile.write(response) #send response

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in a separate thread."""

if __name__ == "__main__":
    server = ThreadedHTTPServer(('localhost', 1337), Handler)
    print('Starting server, use <Ctrl-C> to stop')
    server.serve_forever()
