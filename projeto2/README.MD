# SO PROJECT 2
Raquel Grãos Rodrigues 106987
Guilherme Ribeiro Pereira 107057

We divided the files as follows:
- main: initiates the program, creates the server's pipe, the host thread and the worker threads;
- hostFn: handles everything related to the host thread, including the reading from the server's pipe;
- sessionFn: handles everything related to the worker threads, such as the reading and writing of the client's pipes;
- pathQueue: handles everything related to the producer-consumer buffer;

In order to run the program, the following must be written to the according terminals:

- Server side: ./ems server_pipe_path

- Client side: ./client request_pipe_path response_pipe_path server_pipe_path ../jobs/job_file