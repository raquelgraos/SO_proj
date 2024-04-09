# SO PROJECT 1
Raquel Grãos Rodrigues 106987
Guilherme Ribeiro Pereira 107057

We divided the files as follows:
- main: initiates the program and creates processes;
- processFile: Handles everything related to the behavior of each process, including the creation of threads;
- threadFn: Handles everything related to the behavior of each thread.

Choice of locks:
    -We chose to lock the entire layout of the event instead of each seat individually because we believe that blocking the seats would add a significant amount of complexity to the code without necessarily reflecting greater efficiency, especially in cases where there are events with many seats, such as a 300x300 event.