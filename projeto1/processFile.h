#ifndef PROCESS_FILE_H
#define PROCESS_FILE_H

/// Frees everything in the arhuments structure
/// @param args the list of arguments
/// @param MAX_THREADS Maximum number of simultaneous threads
void free_args(args_t* args, int MAX_THREADS);

/// Writes to output
/// @param fd File descriptor
/// @param to_write What will be written
/// @return 0 if successful, -1 on error.
int write_file(int fd, char* to_write);

/// Removes the extension from the path
/// @param job_path_dup the path
/// @return path without extension
char* remove_path_extension(char job_path_dup[]);

/// Checks if its a .jobs file
/// @param name the path
/// @return 1 if it is a .jobs file, 0 it is not
int check_file_extension(char *name);

/// Adds a line to the list of lines with wait and the respective delay to the list of delays
/// @param args argument to add the list of wait lines 
/// @param line the line
/// @param delay the delay
/// @return 0 if successful, -1 on error.
int add_to_wait_list(args_t* args, int line, unsigned int delay);

/// Adds a line to the list of lines with barrier
/// @param args argument to add the list of barrier lines
/// @param line the line
/// @return 0 if successful, -1 on error.
int add_to_barrier_list(args_t* args, int line);

/// Initializes the lists of wait lines and barrier lines
/// @param args the list of arguments
/// @param job_path the path to the directory
/// @param MAX_THREADS Maximum number of simultaneous threads
/// @return 0 if successful, -1 on error.
int initialize_lists(args_t* args, const char *job_path, int MAX_THREADS);

/// Processes the file and creates the threads
/// @param job_path the path to the directory
/// @param MAX_THREADS Maximum number of simultaneous threads
void process_file(const char *job_path, int MAX_THREADS);


#endif  // PROCESS_FILE_H

