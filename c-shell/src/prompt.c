#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

/*Using static variable to store the path of shells home directory and marking it static cuz 
it then restricts its visibility to this sourse file and PATH_MAX is the maximum length for 
the file path according to POSIX*/
static char home_dir[PATH_MAX];

/*When the shell starts then remember the directory where I have started That is treated as 
shells home directory (~) - This is the main job of this below function*/
void init_prompt(void) {
    /* getcwd (get cuerrent working directory) copies the absolute path of the current working 
    directory into home directory that is the first arg in getcwd stores the path f the home_dir*/
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        /* If system call fails, then the system error will pop up and terminate process */
        perror("getcwd failed during initialization");
        exit(EXIT_FAILURE);
    }
}

const char *get_home_dir(void) {
    return home_dir;
}

/* This function does all the printing <username@hostname:currentpath> stuff
cwd holds the pathname and hostname hold the laptops or devices name 
and username stors the current users login name*/
void display_prompt(void) {
    char cwd[PATH_MAX];
    char hostname[HOST_NAME_MAX];
    char username[LOGIN_NAME_MAX];
    /*Here we check if the username exists or not first and it it exits then it prints or else
    if it fails it asks the uid of the process(uid) and information(pwuid) related to the uid and there 
    is a struct related to the information and the fetchig of the username will happen here or
    even at this point if the username is not being able to be fetched then the username is unknown */
    if (getlogin_r(username, sizeof(username)) != 0) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_name) {
            strncpy(username, pw->pw_name, sizeof(username) - 1);
            username[sizeof(username) - 1] = '\0'; // Ensure null-termination
        } else {
            // Ultimate fallback if user record cannot be resolved
            strncpy(username, "unknown", sizeof(username) - 1);
            username[sizeof(username) - 1] = '\0';
        }
    }

    //Here the hostname will be fetched and HOST_NAME_MAX id in limits.h lib
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    //Here the current working directory is retrieved and dynamically fetches the directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(cwd, "unknown", sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';
    }

    /*Does relative path formatting that is it compares the current working directpry with home 
    directory and then replaces the home directory with ~ and to replace it with ~ the directory path 
    should end with '/' or '\0'*/
    char formatted_path[PATH_MAX];
    size_t home_len = strlen(home_dir);

    if (strncmp(cwd, home_dir, home_len) == 0 &&
       (cwd[home_len] == '/' || cwd[home_len] == '\0')) {
        // remove the home_dir prefix and prepend with ~
        // (cwd + home_len points to the remainder of the path after home_dir)
        snprintf(formatted_path, sizeof(formatted_path), "~%s", cwd + home_len);
    } else {
        // If outside home directory tree, keep full absolute path
        snprintf(formatted_path, sizeof(formatted_path), "%s", cwd);
    }

    //final print statement that is it prints text to terminal
    printf("<%s@%s:%s> ", username, hostname, formatted_path);
    /*without this the command stays in buffer (because of the space) now with this 
    the print command goes to terminal and as next line is not needed as we need to 
    put the cursor in the same line'\n' is not needed*/
    fflush(stdout);
}