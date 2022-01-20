const WebSocket = require('ws');
const server = new WebSocket.Server({
  port: 8080
});

let counter = 0 ; 
let sockets = [];
server.on('connection', function(socket) {
  sockets.push(socket);

  // When you receive a message, send that message to every socket.
  socket.on('message', function(msg) {
    console.log(msg.toString());
    sockets.forEach(s => s.send(msg));
  });

  // When a socket closes, or disconnects, remove it from the array.
  socket.on('close', function() {
    sockets = sockets.filter(s => s !== socket);
  });
});


// Every x seconds, send a message to all connected sockets
setInterval(function() {
  sockets.forEach(function(socket) {
    socket.send('[' + counter + ']');
    counter += 1;
  });
}, 1000);