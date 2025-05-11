https://datatracker.ietf.org/doc/html/rfc1459

- when client ^Z, remove client from server list -> need a heartbeat mechanism (or keep-alive check) on the server
- fix messaging & error catching:
my> /connect bob
Connected to bob.
my> Hi bob
my>
bob: Hi how are you?
my> Testing my chat program
my>
bob: Oh ok
my> Bye
my> ^C
Left conversation with bob.

my> /connect Alice
Error, cannot connect in wait mode.
my> /quit


how to run
./server 1234

./client student1
Client student1 connected.
student1> /
./client student1
Client ID is already taken.

./client student1
Client student1 connected.
student1> /list
No clients are waiting.
student1> /wait
Waiting for connection. (Ctrl+C to stop waiting)
student1> ^C
Stopped waiting.
student1> 

./client student1
Client student1 connected.
student1> /list
No clients are waiting.
student1> /wait
Waiting for connection. (Ctrl+C to stop waiting)
student1> 
You're connected!
student1> hi
student1> 
student2: hello
student1> byte
student1> ^C
Left conversation with <>.
student1> /list
No clients are waiting.
student1> /quit
./client student2
Client student2 connected.
student2> /list
No clients are waiting.
student2> /list
Waiting clients:
student1
student2> /connect student1
Connected to student1.
student2> 
student1: hi
student2> hello
student2> 
student1: byte
student2> 
The other client has left conversation.
student2> /list
No clients are waiting.
student2> /quit







fix client to hold other client_id, and fix "%s: %s" formatted
so that /quit is simple





blocking behavior is acceptable for /connect
and /wait



peer_info in handle_signal, /quit -> need global?
receiving messages with client_socket, new receive_messages?




difference between using peer_info within main outside of /wait and /connect
and within /wait and /connect



to do this..
my> /wait
my> /connect Alice
Error, cannot connect in wait mode.
make accept non-blocking:
```
// ✅ Make `accept()` Non-Blocking Using `fcntl()`
    int flags = fcntl(client_info.listener_socket, F_GETFL, 0);
    fcntl(client_info.listener_socket, F_SETFL, flags | O_NONBLOCK);
```
or
run /wait accept in a thread
or select()



Should select() Be Used for /wait?
✅ For /connect: Yes, because the "CONNECT %s %s %d" message must be processed in main(), and recv() in receive_server_messages() would otherwise consume it first.
🔶 For /wait: Optional, because:

/wait uses accept(), which only blocks if no connection is incoming.
If accept() runs in a separate thread, the main loop remains free to handle other commands.
If we keep /wait inside main(), then select() should be used to avoid blocking.
✅ Best Approach for /wait
There are two options:

Use a Separate Thread for /wait (Recommended for simplicity)
Modify main() to use select() for accept() (Better for a fully event-driven model)


If /wait is handled in a separate thread, then select() is not needed, because the main loop remains unblocked.



on second thought, i don't think i need the select in main() for server vs peer retrieval
I just need to place receving CONNECT in the server receiving thread