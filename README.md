HW05: Chat Client (due: 2016-04-19, individual)
===
Implement a chat server and client.

Server
====
* The server should support at least 4 simultaneous clients.
* Parameters could be passed to the server to indicate what port it should be bound and what directory to store/retrieve the files.

Client
====
* Upon start-up, the client chooses an IP and Port to connect to.
* When connected, the client prompts the user for a username. (No need to check for collisions but a bonus)
* Typing text just sends it, but commands may be issued.
* Commands start with a slash `/` followed by the keyword, optionally followed by a space and parameters. The commands are:
  * `/ls` – List all the files on the server.
  * `/upload [file]` – Upload the file to the server, upon name collision, the server adds a `(n)` suffix on the filename, just before the extension. For example, if two users upload `img.png`, the latter one gets the name `img(2).png`
  * `/download [file]` – Downloads a file from the server.
* Additional commands may be implemented as bonus.
* Bonus points for interoperating with another server.
