/*
Clustershell client protocol:
1. Client is opened on any machine that wishes to be part of the cluster
2. Client connects to server on TCP
3. Client sends command to server, and server coordinates the connected clients to execute the command.
4. Client receives final output to the client which sent the command

Message Design:
6 digit header + message, as follows:
To server: [c/o][mlength][command/output string]
To client: [c/o][mlength][(if c) i][(if c)ilength][input string][command/output string]
c - command
o - output
mlength - 5 digit number signifying length of rest of message
(only if command to client) i - input 
(only if command to client) ilength - 3 digit number, length of input string

Assumptions:
1. All clients listed in the config file connect in the beginning itself and none of them leave before all commands are over
2. There are no commands that require manual user input from the shell (stdin)
3. Commands and outputs are of maximum string length 99999 include all ending characters and newlines. 
(If output is of greater length, then it will be cutoff at that point.)
4. Nodes are named as n1, n2, n3, ... nN.
*/