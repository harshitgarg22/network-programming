/*
Clustershell client protocol:
1. Client is opened on any machine that wishes to be part of the cluster
2. Client connects to server on TCP
3. Client sends command to server, and server coordinates the connected clients to execute the command.
4. Client receives final output to the client which sent the command
*/