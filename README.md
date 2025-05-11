# Network Communication Protocol Tools

A collection of custom client-server implementations in C demonstrating key internet protocols and concurrent communication using sockets, threads, and synchronization primitives.

## Includes
- **FTP/TFTP:** Parallel file chunk downloads from multiple servers; chunk reassembly handled client-side. UDP version handles basic retransmission.
- **HTTP/HTTPS:** Emulates wget; supports GET/HEAD, uses DNS lookup, SSL/TLS for secure transfers.
- **IRC Chat:** Clients can register with server, enter wait or info mode, and initiate peer-to-peer conversations. Inspired by RFC 1459.

## Technology Used
- C
- POSIX Sockets (TCP/UDP)
- Pthreads, mutexes, semaphores
- OpenSSL (for HTTPS)
- DNS resolver library (getaddrinfo)

## Features
- Multi-threaded transfer handling
- Peer-to-peer chat state machine
- SSL/TLS secure communication
- DNS hostname resolution
- Thread-safe shared resource access

## References
- RFC 1350 – TFTP Protocol
- RFC 1459 – IRC Protocol
- OpenSSL docs

## Future Improvements
- Add retry logic to TFTP for better packet loss handling
- Improve IRC server performance with non-blocking I/O
- Add unit tests and more graceful shutdown handling
- Additionals? sctp, ftps, sftp, nfs