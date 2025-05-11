## How to Transition to UDP
To make your implementation closer to UDP, you’ll need to:

1. Replace the connection-oriented connect() and accept() with connectionless sendto() and recvfrom().

2. Implement:
    - Sequence Numbers: To handle out-of-order delivery.
    - Acknowledgments: To ensure data is received.
    - Retransmissions: To recover lost packets.

3. Modify your protocol:
    - Add sequence numbers and acknowledgments to your CHECK and GET commands.
    - Split file chunks into smaller packets suitable for UDP transmission (~1400 bytes).

## Key Steps for Implementation

1. Define the Application-Layer Protocol
    You need to specify:
    - Commands and Responses:
        - CHECK <filename>: To check file existence and get file size.
        - GET <filename> <offset> <chunk_size>: To request a specific chunk.
    - Sequence Numbers: Add sequence numbers to each UDP packet to ensure ordering.
    - Acknowledgments: Use ACK <seq_num> from the client to the server to confirm receipt of each packet.
    - Timeouts and Retransmissions: Implement a timer for each packet, retransmitting if an ACK is not received in time.

2. Implement the Server
    1. Listen for Requests:
    - Use recvfrom() on a known port to handle CHECK and GET requests.
    2. Send Chunks Reliably:
    - Break the requested chunk into smaller packets (based on a size like 1024 bytes).
    - Attach a sequence number to each packet.
    - Retransmit packets until the client acknowledges them.
    3. Concurrent Transfers:
    - Use threads or processes to handle multiple clients/chunks concurrently.
    - Each thread/process can open a new UDP port for communication.

3. Implement the Client
    1. File Information Request:
    - Send CHECK to one server to determine the file size.
    - Calculate offsets and chunk sizes based on the file size and num_chunks.

    2. Download Chunks:
    - Use GET to request specific chunks from servers.
    - Receive packets using recvfrom(), reassemble them based on sequence numbers, and send ACK for received packets.

    3. Handle Errors:
    - Retry or switch to another server if a server fails or times out.
    - Re-request missing packets based on sequence numbers.

4. Handle UDP Limitations
    - Fragmentation: Ensure packets are small enough to avoid fragmentation (use ~1400 bytes for payload).
    - Packet Loss: Retransmit lost packets using acknowledgments and timeouts.
    - Out-of-Order Delivery: Use sequence numbers to reorder packets.
    - Duplicate Packets: Track received packets and ignore duplicates.

## Advantages of UDP (Potential Optimizations)

1. Reduced Overhead:
- TCP includes built-in mechanisms for reliability, such as error checking, retransmission, acknowledgment, and flow control, which add overhead.
- UDP is lightweight and does not have these mechanisms, potentially leading to faster communication, especially for applications with minimal reliability requirements.

2. Fine-Grained Control:
- By implementing reliability at the application layer, you can tailor the protocol to your exact needs.
- For example, you can prioritize specific chunks, manage retries differently, or adjust transmission logic dynamically.

3. Parallelism:
- UDP's connectionless nature allows clients to send and receive packets from multiple servers or ports without needing to establish separate connections.
- This can simplify and optimize scenarios with many servers or parallel downloads.

4. Faster Startup:
- UDP skips the connection establishment phase (SYN, SYN-ACK, ACK in TCP), reducing latency at the start of the transfer.

## Disadvantages of UDP

1. Increased Complexity:
- You must manually handle: Packet loss and retransmission. Packet ordering. Error checking and data integrity.
- Implementing these features effectively can be non-trivial and error-prone.

2. Higher Risk of Data Loss:
- Without TCP’s built-in reliability, packets may be lost or corrupted.
- In networks with high packet loss or congestion, UDP may perform worse than TCP.

3. Not Always Faster:
- On reliable networks, TCP’s overhead is minimal, and its optimizations (e.g., congestion control) can outperform UDP-based implementations.
- If you manually implement reliability in UDP, you might reintroduce overhead similar to TCP.

4. Fragmentation Issues:
- If packets exceed the network’s Maximum Transmission Unit (MTU), they get fragmented. Fragmentation increases the risk of loss and retransmission.
- With TCP, fragmentation is handled automatically.

## When UDP Could Be an Optimization

1. Unreliable Networks:
- On lossy networks, TCP can experience delays due to congestion control or excessive retranmissions.
- A custom UDP protocol could allow more aggressive retries or partial success.

2. Real-Time or Low-Latency Requirements: If your project involves time-sensitive data, like streaming or gaming, UDP can reduce delays.

3. High Scalability: UDP’s connectionless nature allows for easier scaling when many clients or servers are involved.

4. Controlled Environment: If you know the network is reliable (e.g., a local network), you might not need TCP's guarantees, making UDP a viable and faster option.

## Is It an Optimization for Your Project?
- If your goal is reliability: TCP is a better fit. Transitioning to UDP would increase complexity without significant performance gains.
- If your goal is speed or scalability in a controlled environment: UDP could be an optimization, but only if implemented carefully.

## Recommendation
If transitioning to UDP is purely to demonstrate your skills:
- Focus on showcasing your ability to implement reliability features (e.g., retransmissions, sequence numbers, acknowledgments).
- Highlight the tradeoffs in your documentation or resume, showing that you understand the pros and cons of each protocol.


### Debuging UDP not getting ACK
```
sudo tcpdump -i any udp port 1024
```
tcpdump: data link type LINUX_SLL2 \
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode \
listening on any, link-type LINUX_SLL2 (Linux cooked v2), snapshot length 262144 bytes \
01:40:07.189189 lo    In  IP localhost.33422 > localhost.1024: UDP, length 22 \
01:40:07.191106 lo    In  IP localhost.1024 > localhost.33422: UDP, length 12 \
01:40:07.192758 lo    In  IP localhost.42797 > localhost.1024: UDP, length 31

1. Client 1's Interaction:
    - Client 1 sends a CHECK request to Server 1.
    - Server 1 processes the CHECK request and responds appropriately.
2. Client 2's Interaction:
    - Client 2 sends a CHECK request to Server 1.
    - After this point, the program hangs.



The server reporting a value of bytes_sent=18446744073709551615 (which is 2^64 - 1) indicates that the sendto() call returned -1, but it is being interpreted as an unsigned value due to incorrect handling of the return type.

What This Means

I got:
Sent seq_num=0, bytes_sent=18446744073709551615

The sendto() function returns a ssize_t value (a signed integer type). If it fails, it returns -1, which, when cast to an unsigned type like size_t, becomes 2^64 - 1 (on a 64-bit system).
You are likely using size_t to store the return value of sendto() instead of ssize_t. As a result, any error (indicated by -1) is misrepresented as an enormous unsigned value.

then I got:
Sent seq_num=0, bytes_sent=18446744073709551615
Error sending data to client: Message too long

UDP Packet Size Limitation:
The theoretical maximum size of a UDP datagram is 65,535 bytes (including headers).
However, in practice, most networks have a much smaller MTU (Maximum Transmission Unit), often around 1,500 bytes for Ethernet.
If the data being sent exceeds this size, the kernel will reject it, resulting in the Message too long error.

Why This Happens in Your Code:
You're using a large buffer size (BUFFER_SIZE = 1048576, or 1 MB).

Limit BUFFER_SIZE to a more reasonable value, such as 1,024 bytes or 1,400 bytes.
Ensure bytes_to_read respects the BUFFER_SIZE limit

Standardize ACK formats

Ensure Ports Match, Check and bind the client to a consistent port if necessary
Do I need to do this? Ports seem to be ephermeral
``` in client.c
struct sockaddr_in client_addr;
client_addr.sin_family = AF_INET;
client_addr.sin_port = htons(<fixed_port>); // Choose a unique port
client_addr.sin_addr.s_addr = INADDR_ANY;

if (bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
}
```

how to decide on ACK seq #s?

It seems like the while(1) loop in server listens for requests from client. It seems like it mistakenly receives the ACK from client as a new request to handle, instead of waiting for client ACK in the same thread within the handling function

The issue arises because the server’s main loop recvfrom() indiscriminately intercepts all incoming packets, including ACKs meant for specific threads. This behavior causes the ACKs to be handled as new requests or misplaced, resulting in threads waiting indefinitely for acknowledgments.

Why This Happens
Thread Context Mismatch: The ACK packets are processed by the main recvfrom() loop instead of being captured in the thread that is waiting for them.
Lack of Packet Routing: There is no routing mechanism to ensure that ACK packets are directed to the specific thread handling the associated GET reques


Currently, all threads are sharing the same server socket (request->server_socket). When multiple threads call recvfrom() on the same socket, they can interfere with one another.


Solution: Use SO_REUSEADDR
Before binding the thread's dedicated socket, set the SO_REUSEADDR option. This allows multiple sockets to bind to the same address and port.
Create a dedicated socket for each thread inside handle_request():
Why SO_REUSEADDR is Needed:
By default, each socket must bind to a unique (IP, port) tuple.
SO_REUSEADDR allows multiple sockets to bind to the same address/port while avoiding conflicts.
Addressing Unique Communication:
Even with shared ports, the recvfrom() call distinguishes packets by their source address (client IP and port). This ensures each thread processes its client's packets correctly.
Thread-Specific Sockets:
Threads now create their own sockets but share the same server address/port. The source IP and port of the client remain unique, ensuring no mix-ups.

Alternative: Use a Single Socket with Synchronization
If creating a dedicated socket for each thread becomes too complex, another approach is to use the same socket for all threads and synchronize access using a mutex.
```
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

// In thread
pthread_mutex_lock(&socket_mutex);

// Use the shared socket for recvfrom() and sendto()
ssize_t bytes_received = recvfrom(shared_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
// Process received data
sendto(shared_socket, response, response_len, 0, (struct sockaddr *)&client_addr, addr_len);

pthread_mutex_unlock(&socket_mutex);
```
While this method introduces a bottleneck due to the mutex, it simplifies socket management and avoids issues with address reuse.

However, in most UDP-based server implementations, the server typically does not need a mutex for the socket because:
Each recvfrom call processes an independent request.
Responses are typically stateless, and UDP sockets are inherently connectionless.
- but we're making this reliable with ACK..

using ACKs (Acknowledgments) is necessary to ensure reliability when building a reliable application-layer protocol over UDP. Since UDP is connectionless and does not guarantee delivery, ACKs allow you to confirm receipt of each packet, implement retransmissions, and maintain proper sequencing.

Implementation Considerations
Given the requirement, you have two main options for implementation: mutex and SO_REUSEPORT. Let's evaluate them in this context.

1. Using a Mutex
How it works: A single UDP socket is shared across multiple threads on the client or server. Access to the socket for sendto and recvfrom is synchronized with a mutex to ensure thread-safe operations.

Advantages:

Simplifies socket management by using only one socket.
Avoids issues like "Address already in use" and "Permission denied" when binding multiple sockets to the same port.
Disadvantages:

Increased contention: Threads must wait for access to the shared socket, which can slow down performance.
Complex locking logic: It may require additional locks for shared resources (e.g., logs, buffers).
When to Use Mutex
Use mutexes if:

You want to keep the implementation simple and manageable.
You don't expect high contention on the socket (e.g., a small number of threads or moderate traffic).
2. Using SO_REUSEPORT
How it works: Each thread gets its own dedicated UDP socket bound to the same port. The SO_REUSEPORT option allows multiple sockets to bind to the same address and port. The kernel handles incoming traffic by load-balancing packets to the correct socket.

Advantages:

Avoids contention: Each thread can independently send and receive data on its socket.
Scalable: Better suited for high-throughput or high-concurrency applications.
Disadvantages:

SO_REUSEPORT might not be available on all platforms or require elevated privileges.
Additional complexity in managing multiple sockets and ensuring correct thread assignment.
When to Use SO_REUSEPORT
Use SO_REUSEPORT if:

You are implementing a high-performance server or client with many threads.
Your platform supports SO_REUSEPORT without issues.
Acknowledgments (ACKs)
Regardless of the chosen approach, using ACKs is essential for reliability:

For the Server:

After sending a chunk, wait for an ACK from the client.
Retransmit the chunk if the ACK is not received within a timeout.
Use sequence numbers to match ACKs with chunks.
For the Client:

Send an ACK after receiving a chunk to confirm its receipt.
Ensure that ACKs include the sequence number of the chunk being acknowledged.
Stop-and-Wait Protocol
A stop-and-wait protocol is suitable for this project as it:

Sends one chunk at a time.
Waits for an ACK before proceeding to the next chunk.
Retransmits if the ACK is not received.
Which Option to Choose?
If you're learning or implementing a prototype, start with a mutex. It simplifies socket management and ensures correctness.
If you're building a high-performance or production-grade solution, consider using SO_REUSEPORT to improve concurrency and scalability.

If we're using one socket in server to receive messages, why does main received the ack and ignores it whereas the request handler doesn't receive the ack (and it's listening on the same port) and logging that no ack was received


The issue arises because the server's main loop and the handle_request function are both using the same socket to receive messages. UDP sockets do not differentiate between who is waiting to read data—they simply deliver data to the first recvfrom() call that is executed. This causes a race condition where either:

The main loop intercepts the ACK message:

The recvfrom() call in the main loop reads the ACK intended for the handle_request thread. Since the main loop is not responsible for processing ACKs, it logs the message and continues, effectively "stealing" the packet.
The handle_request function misses the ACK:

Since the ACK is already consumed by the main loop, the recvfrom() in the handle_request function times out and concludes that no ACK was received.