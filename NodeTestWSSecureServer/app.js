const fs = require('fs');
const tls = require('tls')

// NOTE: Generate and setup your own certificate and private key here
const privateKey = fs.readFileSync('key.pem');
const cert = fs.readFileSync('cert.pem');
var creds = {key: privateKey, cert: cert, passphrase: 'Chipkintest'};

let counter = 0 ; 
let sockets = [];

// Setup TLS server
var server = tls.createServer(creds, function(socket) {
  sockets.push(socket);

  // When you receive a message, send that message to every socket.
  socket.on('data', function(data) {
    console.log(data.toString());
    sockets.forEach(s => s.write(data));
  });

  // When a socket closes, or disconnects, remove it from the array.
  socket.on('end', function() {
    sockets = sockets.filter(s => s !== socket);
  });

  // When socket errors, print the error and remove it from the array.
  socket.on('error', function(error) {
    sockets = sockets.filter(s => s !== socket);
    console.error(error);
  });
});

server.listen(8443, 'localhost');

// Print error if any
server.on('error', function(error) {
  console.error(error);
})

// Every x seconds, send a message to all connected sockets
setInterval(function() {
  sockets.forEach(function(socket) {
    socket.write('[' + counter + ']');
    counter += 1;
  });
}, 1000);