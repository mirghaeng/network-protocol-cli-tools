SSL certificate chain

Adding missing certificates to your chain can be a valid approach in certain scenarios, but whether it is the right thing to do depends on your specific use case, the system’s security requirements, and the context in which the certificates are being used.

Here’s an analysis of when it is appropriate and the implications:

When Adding Missing Certificates is Appropriate
Incomplete Server Certificate Chain:

Some servers do not send intermediate certificates in their chain, assuming clients already have them.
In this case, adding the missing intermediates to your chain or CA bundle is a legitimate solution.
Custom CA Certificates:

If you are working in an environment with self-signed certificates or internal CAs, adding these certificates to your CA bundle or trust store is necessary for verification.
Temporary Workaround:

If the server configuration cannot be fixed quickly, adding the missing certificates can serve as a stopgap measure until the server's certificate chain is corrected.
When It Might Not Be the Right Thing
Server Misconfiguration:

If the server doesn’t send a complete certificate chain, the correct approach is to fix the server. Servers are responsible for sending their certificate along with all intermediates required to build the chain up to a trusted root CA.
Security Implications:

Adding certificates manually increases the risk of inadvertently trusting unverified or compromised certificates.
If the added certificates are not from a reliable source, this could introduce a security vulnerability.
Scalability Issues:

If multiple clients or systems connect to the server, manually adding certificates to each client is impractical. Fixing the server to provide the complete chain is more scalable.
Alternatives to Adding Certificates
1. Fix the Server Configuration
Configure the server to send the full chain (server certificate + intermediates).
This is the best long-term solution and ensures clients don’t need to add certificates manually.
2. Use a Standard CA Bundle
Ensure your program uses the default system CA bundle:
c
Copy
Edit
if (!SSL_CTX_set_default_verify_paths(ctx)) {
    fprintf(stderr, "Failed to use default verify paths.\n");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}
If the intermediates are in the default bundle, verification will succeed without additional changes.
3. Dynamically Handle Missing Certificates
If you cannot control the server, dynamically retrieve and use missing certificates without permanently adding them to your CA bundle:

Extract the chain from the server using:
bash
Copy
Edit
openssl s_client -connect www.example.com:443 -showcerts
Use the intermediate certificates dynamically in your program without modifying the CA bundle:
c
Copy
Edit
if (!SSL_CTX_load_verify_locations(ctx, "intermediate.pem", NULL)) {
    fprintf(stderr, "Failed to load intermediate certificate.\n");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}
4. Bypass Verification Temporarily
For testing purposes, you can bypass verification, but never use this in production:
c
Copy
Edit
SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
Best Practices
Fix the Server:

Ensure the server provides a complete chain, including intermediate certificates.
Use tools like SSL Labs Test to verify the server configuration.
Use Trusted Sources for Certificates:

Only add certificates from reliable sources like Certificate Authorities (CAs).
Avoid Permanent Modifications:

If you must add certificates, prefer dynamic or local modifications to avoid unintended global trust.
Regularly Update CA Bundles:

Ensure your CA bundle is up-to-date to avoid issues with expired or missing certificates:
Linux: Use update-ca-certificates or update-ca-trust.
Custom: Download the latest bundle from Curl's CA Extract.
When to Add Certificates
Yes: When using custom CAs or self-signed certificates that are explicitly trusted in your environment.
Yes: As a temporary measure for incomplete server chains.
No: When the server misconfiguration can be corrected.
No: If the source of the missing certificates cannot be trusted.
Conclusion
Adding missing certificates to your chain is acceptable in certain cases, but the ideal solution is to ensure the server sends a complete and valid certificate chain. This avoids security risks, reduces maintenance overhead, and ensures compliance with standard practices.



SSL Certifications Chain
https://youtu.be/msBrdFiSvW4?si=KuN8XgcqUXQm_Oux