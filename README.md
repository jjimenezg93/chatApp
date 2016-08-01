# chatApp
Chat application developed in C++ (using WinSockets).

It consists on:

- Server: has n+2 threads. One thread receives new connections, stores client's info and creates a new thread for each client, which will receive their messages. Lastly, there's a reader thread, which reads chat and checks what was the last message each client received, keeping every client updated by sending them all the new messages.

- Client: has 2 threads. One asks for name and stablishes connection with the server, besides creating a reader thread. The reader receives all new messages from the server.


Both sides are safe against abrupt disconnections.
