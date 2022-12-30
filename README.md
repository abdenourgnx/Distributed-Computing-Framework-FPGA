# Distributed-Computing-Framework-FPGA

This project is a distributed system developed in C++ that offers users a framework to configure and interface with multiple FPGA boards in a distributed computing environment. The system consists of a server connected to several Terasic DE10 FPGA boards on a local network, while a custom messaging protocol built on top of TCP/IP is used for communication.

One of the main goals of this project is flexibility, as it is designed to adapt to various application requirements. The system is also built with scalability in mind, as it can support multiple users and multiple computing nodes. The project makes use of the Boost library to aid in the asynchronous networking stack.

To test the system, a load-free performance benchmark was conducted on each component separately to observe the system overhead on the user application. Using a matrix multiplier test-case application, the system was tested with no networking hardware limitation. The results showed a low overhead on the user application, with a logical behavior. As more computing hardware was incorporated, the total throughput increased up to 8.6 Gbps and the latency overhead decreased.

A comparison with a software implementation of the same test-case application was also conducted, and the system performed up to 1.3, 5.5, and 11.5 times faster using 4, 16, and 36 nodes, respectively, compared to the software-based implementation.
