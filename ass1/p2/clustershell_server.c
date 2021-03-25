/*
Clustershell server protocol:
1. Server is run on 1 pre selected IP
2. Server keeps waiting for connection from clients, and maintains a list of connected clients
3. Upon recieving a command from any client, it coordinates the connected machines to execute it
4. Returns final output to the client
*/