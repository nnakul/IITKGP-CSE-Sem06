
// CS39002 Operating Systems Laboratory
// ASSIGNMENT 02 | Use of System Calls
//      Nakul Aggarwal (19CS10044)
//      Hritaban Ghosh (19CS30053)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <termios.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <stack>
#include <algorithm>
#include <map>
#define __hist_size__   10000
#define __hist_print_size__ 1000
#define __ctrl_plus_R__ 18
#define __ctrl_plus_C__ 3
#define __backspace__ 127
#define __tab__ 9
#define __non_printable_ascii_lim__ 31
#define __hat_sq_bracket__  27
#define __wait_time__   2
#define __print_timeout__ 0
#define ULL unsigned long long
using namespace std ;

int child_pid = 0 ;
int history_of_cmds_size = 0 ;
list<string> history_of_cmds ;
vector<int> multiwatch_child_pids ;
sighandler_t default_SIGTSTP_handler ;
sighandler_t default_SIGSEGV_handler ;
unordered_map<string, int> history_of_cmds_unq ;
static struct termios oldtio , newtio ;
int readstate = false ;
int bgid = 1 ;
string bg_procs_default_outfile = "bg_procs_" + to_string(time(NULL)) ;
string multiwatch_redirect_outfile ;
unordered_map<int, tuple<long, int, string>> all_backg_procs ;

// Strips whitespaces from the start and end of the string given to it as argument
void Strip ( string & );

// Returns true if the character is a whitespace or a tab else false
int IsWhiteSpace ( char );

// Returns the length of the longest common subsequence
int LCSLength ( const string &, const string &);

// Returns true if the command has a non-printable character (Ex: Ctrl) else false 
int HasNonPrintableChar ( const string & );

// Erases a specified number of characters from the stdout buffer stream
void Backspace ( int );

// Read the search term from the user from the console and return appropriate string
string ReadSearchTerm ( );

// Reads a numeric value from the console and returns it
int ReadNum ( );

// Prints all the commands with the particular search term (whole word then longest common subsequence) and then asks the user for the command they want to select and returns the same
string SearchHistory ( );

// Prints the history of commands that is maintained
void PrintHistory ( int );

// Print information regarding the running background processes in a neatly formatted table
void PrintRunningBackgroundProcs ( );

// Print information regarding the killed background processes in a neatly formatted table, prevmode indicates whether or not to print the header of the table
void PrintKilledBackgroundProcs ( int );

// Initializes the global variables list<string> history_of_cmds and unordered_map<string, int> history_of_cmds_unq 
// Loads the bash history of commands into the shell program
int Init ( );

// Prints the shell prompter in a beautiful format along with the CWD if print is set to 1.
// Returns the length of the shell prompter
int CurrentWorkingDir ( int );

// Returns whether or not a character is a delimiter
int IsDelim ( char );

// Returns the lowest common prefix among the vector of strings
string LCP ( const vector<string> & );

// Autocompletes the string on the console with names from CWD or if impossible lists the CWD
// Also returns a status marker which shows which path was chosen
string AutoComplete ( const char * , int * );

// Read the command from the user from the console and return appropriate string
string ReadCmd ( );

// Returns true if the command has a pipe character ('|') else false
int IsPiped ( const char * );

// Returns the vectorized form of the array
char ** ParseNonPipedCmd ( const char * );

// Returns the vector of commands taking part in the pipe command
vector<char **> ParsePipedCmd ( const char * );

// Returns the command string reconstructed from the array of args
string GetCommand ( char ** );

// Creates a child process and execute a sub-command of the piped command
int ExecPipedCmd ( char ** , int , int );

// Creates a pipe messaging system to communicate between the child processes of the pipe
int ExecPipedCmd ( vector<char **> &  , int );

// Creates a child process and executes the command. It first check whether its a background process or not and takes appropriate steps.
// We also introduce some new commands to keep track of background process whose output is stored in a file
int ExecNonPipedCmd ( char ** , int , const string & );

// Updates the bash history
int UpdateBashHistory ( );

// Stops all processes and switches off the read_state
void Stop ( int );

// Stops all multiwatch processes and raises an appropriate signal
void Exit ( int );

// Refines the multiwatch commands taking care of opening and ending square brackets and double quotes
// Therefore modifying the vector of multiwatch commands
void RefineMultiWatchCmds ( vector<string> & );

// Executes a component command of the multiwatch watch command and redirects its output to the correct file descriptor
int ExecMultiWatchCmd ( const char * , int );

// Returns true if the command is a multiwatch command in the correct format else false
int IsMultiWatchCmd ( char * & );

// Returns true if the file name is valid (Checking of matching double quotes and single quotes) else false
int ParseFileName ( string & );

// Parses the Multiwatch command and return the vectorized form of the multiwatch command and set the redirecting outfile for multiwatch
vector<string> ParseMultiWatchCmd ( const char * );

// Executes the multiwatch commands one by one and waits for all child processes to complete
int MultiWatch ( char * );

// Raises SIGTSTP
void VoluntaryExit ( );

// Set the Keyboard to new terminal settings
void SetKeyboard ( );

// Reset the Keyboard to old terminal settings
void ResetKeyboard ( );

// Setups the terminal to take a command and performs the appropriate procedure
void Home ( );

void Strip ( string & str ) {
    int l = str.size() ;

    // Strip whitespaces from the end of the string
    while ( l && str[l-1] == ' ' ) {
        str.pop_back() ;
        l -- ;
    }
    
    l = str.size() ;

    if ( ! l )  return ;
    if ( str[0] != ' ' )    return ;

    // Strip whitespaces from the beginning of the (original) string
    reverse(str.begin(), str.end()) ;
    while ( l && str[l-1] == ' ' ) {
        str.pop_back() ;
        l -- ;
    }
    reverse(str.begin(), str.end()) ;
}

int IsWhiteSpace ( char c ) {
    return (c == ' ' || c == '\t') ;
}

int LCSLength ( const string & term , const string & cmd ) {
    int n = term.size() , m = cmd.size() ;
    vector<vector<int>> dp (n, vector<int>(m, 0)) ;

    // res indicates the length of the longest common subsequence
    int res = 0 ;

    // Algorithm to calculate the length of the longest common subsequence
    for ( int i = 0 ; i < n ; i ++ ) {
        if ( term[i] == cmd[0] )
            { dp[i][0] = 1 ; res = 1 ; }
    }

    for ( int i = 0 ; i < m ; i ++ ) {
        if ( term[0] == cmd[i] )
            { dp[0][i] = 1 ; res = 1 ; }
    }

    for ( int i = 1 ; i < n ; i++ ) {
        for ( int j = 1 ; j < m ; j++ ) {
            if ( term[i] == cmd[j] ) {
                dp[i][j] = 1 + dp[i-1][j-1] ;
                res = max<int>(res, dp[i][j]) ;
            }
        }
    }
    return res ;
}

int HasNonPrintableChar ( const string & cmd ) {
    // Non-printable characters have ascii values less than or equal to 31

    for ( char c : cmd )
        if ( c <= __non_printable_ascii_lim__ ) return true ;
    return false ;
}

void Backspace ( int d ) {
    // The usual way of erasing the last character on the console is to use the sequence "\b \b". 
    // This moves the cursor back one space, then writes a space to erase the character, 
    // and backspaces again so that new writes start at the old position. 
    // Note that \b by itself only moves the cursor.

    while ( d -- ) 
        printf("\b \b") ;
}

string ReadSearchTerm ( ) {
    printf("\n Enter search term : ") ;
    char c ;
    string term = "" ;
    int cursorx = 21 ;

    // Keep reading characters until newline character is detected (user presses enter) while taking care of non-printable characters
    while ( (c = getchar()) != '\n' ) {
        if ( ! readstate || c == __ctrl_plus_C__ ) {
            printf("\n") ;
            term.clear() ;
            break ;
        }

        if ( c == __tab__ ) {
            int jmp = 8 - cursorx % 8 ;
            if ( ! jmp )    jmp = 8 ;
            Backspace(jmp) ;
            continue ;
        }
        
        if ( c == __backspace__ ) {
            Backspace(2) ;
            int l = term.size() ;
            if ( l > 0 && term[l-1] <= __non_printable_ascii_lim__ )  { Backspace(1) ; cursorx -- ; }
            if ( l ) { term.pop_back() ; Backspace(1) ; cursorx -- ; }
            continue ;
        }

        if ( c <= __non_printable_ascii_lim__ ) cursorx ++ ;
        cursorx ++ ;
        term.push_back(c) ;
    }
    
    if ( HasNonPrintableChar(term) )  return "" ;
    return term ;
}

int ReadNum ( ) {
    char c ;
    string num = "" ;
    printf(" > ") ;
    
     // In a loop keep reading characters from the terminal until user presses enter while making sure it is in fact a number and taking care of non-printable characters
    while ( (c = getchar()) != '\n' ) {
        if ( ! readstate ) {
            printf("\n") ;
            num.clear() ;
            break ;
        }

        if ( c == __ctrl_plus_C__ ) { num.clear() ; break ; }

        if ( c == __tab__ ) {
            Backspace(13) ;
            printf("\r") ;
            printf(" > %s", num.c_str()) ;
            continue ;
        }
        
        if ( c == __backspace__ ) {
            Backspace(2) ;
            int l = num.size() ;
            if ( l ) { num.pop_back() ; Backspace(1) ; }
            continue ;
        }

        if ( ! (c >= '0' && c <= '9') || num.size() == 9 ) {
            Backspace(1) ;
            if ( c <= __non_printable_ascii_lim__ ) Backspace(1) ;
            continue ;
        }

        num.push_back(c) ;
    }
    
    if ( ! num.size() ) return -1 ;
    return atoi(num.c_str()) ;
}

string SearchHistory ( ) {
    string term = ReadSearchTerm() ;
    Strip(term) ;

    if ( ! term.size() )  {
        printf(" [ No match for search term in history. ]\n") ;
        return "" ;
    }   

    if ( history_of_cmds_unq.find(term) != history_of_cmds_unq.end() ) {
        printf(" [ Exact match found for search term. ]\n") ;
        return term ;
    }

    if ( term.size() < 3 ) {
        printf(" [ No match for search term in history. ]\n") ;
        return "" ;
    }

    unordered_map<int, vector<string>> lcs ;
    // lmax records the maximum of the lengths of the longest common subsequences
    int lmax = -1 ;
    for ( const auto & cmdfreq : history_of_cmds_unq ) {
        auto & cmd = cmdfreq.first ;
        int l = LCSLength(term, cmd) ;
        if ( l < 3 )    continue ;
        if ( lcs.find(l) == lcs.end() )
            lcs.insert({l, {cmd}}) ;
        else    lcs.at(l).push_back(cmd) ;
        lmax = max<int>(lmax, l) ;
    }

    if ( lmax == -1 ) {
        printf(" [ No match for search term in history. ]\n") ;
        return "" ;
    }

    auto & cmds = lcs.at(lmax) ;
    if ( cmds.size() == 1 ) {
        printf(" [ Partial match found for search term (%d LCS). ]\n", lmax) ;
        return cmds[0] ;
    }

    multimap<int, string, greater<int>> cmds_sorted ;
    for ( string & cmd : cmds ) 
        cmds_sorted.insert({history_of_cmds_unq.at(cmd), cmd}) ;

    int sno = 1 ;
    for ( auto & freq_cmd : cmds_sorted ) {
        printf(" (%d.)\t%s\n", sno ++, freq_cmd.second.c_str()) ;
    }

    int x = ReadNum() ;
    if ( x < 1 || x >= sno )    return "" ;

    auto it = cmds_sorted.begin() ;
    advance(it, x - 1) ;
    return it->second ;
}

void PrintHistory ( int len ) {
    // Iterate through all the commands in the history of commands and print them along with the serial number
    int sno = 1 ;
    for ( string & cmd : history_of_cmds ) {
        if ( ! len )    break ;
        printf(" %4d | %s\n", sno, cmd.c_str()) ;
        len -- ;    sno ++ ;
    }
}

void PrintRunningBackgroundProcs ( ) {
    // Print information regarding the running background processes in a neatly formatted table
    printf("  PID  |  ENTRY TIME  |        SHELL-GEN. OUT. FILE         | COMMAND \n") ;
    for ( auto & rec : all_backg_procs ) {
        if ( kill(rec.first, 0) != 0 )  continue ;
        
        printf(" %5d | ", rec.first) ;
        printf(" %-11ld | ", get<0>(rec.second)) ;
        string fname = bg_procs_default_outfile + "_" + to_string(get<1>(rec.second)) + ".myshell.out" ;
        printf(" %34s | ", fname.c_str()) ;
        printf(" %s\n", get<2>(rec.second).c_str()) ;
    }
}

void PrintKilledBackgroundProcs ( int prevmode = 0 ) {
    // Print information regarding the killed background processes in a neatly formatted table
    if ( ! prevmode )
        printf("  PID  |  ENTRY TIME  |        SHELL-GEN. OUT. FILE         | COMMAND \n") ;
    
    for ( auto & rec : all_backg_procs ) {
        if ( kill(rec.first, 0) == 0 )  continue ;

        string fname = bg_procs_default_outfile + "_" + to_string(get<1>(rec.second)) + ".myshell.out" ;
        if ( prevmode ) {
            remove(fname.c_str()) ;
            continue ;
        }

        printf(" %5d | ", rec.first) ;
        printf(" %-11ld | ", get<0>(rec.second)) ;
        printf(" %34s | ", fname.c_str()) ;
        printf(" %s\n", get<2>(rec.second).c_str()) ;
    }
}

int Init ( ) {
    // S_IRWXU - This is equivalent to ‘(S_IRUSR | S_IWUSR | S_IXUSR)’.
    // SIRUSR
    // S_IREAD
    // Read permission bit for the owner of the file. On many systems this bit is 0400. S_IREAD is an obsolete synonym provided for BSD compatibility.

    // S_IWUSR
    // S_IWRITE
    // Write permission bit for the owner of the file. Usually 0200. S_IWRITE is an obsolete synonym provided for BSD compatibility.

    // S_IXUSR
    // S_IEXEC
    // Execute (for ordinary files) or search (for directories) permission bit for the owner of the file. Usually 0100. S_IEXEC is an obsolete synonym provided for BSD compatibility.

    int histfd = open(".bash_history.cache", (O_RDONLY | O_CREAT), S_IRWXU) ;
    if ( histfd < 0 ) {
        printf(" ERROR : Initialization process failed.\n") ;
		exit(-1) ;
    }

    // Initializes the global variables list<string> history_of_cmds and unordered_map<string, int> history_of_cmds_unq 
    // Loads the bash history of commands into the shell program
    string cmd = "" ;
    char c ;
    while ( true ) {
        int ret = read(histfd, &c, 1) ;
        if ( ret < 0 ) {
            printf(" ERROR : Bash history could not be retrieved.\n") ;
		    close(histfd) ; return -1 ;
        }
        if ( ret == 0 ) break ;
        if ( c == EOF ) break ;

        if ( c == '\n' ) {
            history_of_cmds_size ++ ;
            history_of_cmds.push_back(cmd) ;
            if ( history_of_cmds_unq.find(cmd) == history_of_cmds_unq.end() )
                history_of_cmds_unq.insert({cmd, 1}) ;
            else    history_of_cmds_unq.at(cmd) ++ ;
            cmd.clear() ;
        }
        else
            cmd.push_back(c) ;
    }
    close(histfd) ;
    return 0 ;
}

int CurrentWorkingDir ( int print = 1 ) {
    // Prints the shell name with the CWD and returns the length of the prompter
	char cwd [1024] ;
	getcwd(cwd, sizeof(cwd)) ;
    if ( print ) {
        printf("\033[1m\033[36m") ;
        printf("\n[MYSHELL]%s$ ", cwd) ;
        printf("\033[0m") ;
    }
    return strlen(cwd) + 11 ;
}

int IsDelim ( char c ) {
    // Returns whether or not a character is a delimiter
    if ( c >= '0' && c <= '9' ) return false ;
    if ( c >= 'a' && c <= 'b' ) return false ;
    if ( c >= 'A' && c <= 'B' ) return false ;

    string delims = "\\\"/'`([{<>" ;
    for ( char d : delims )
        if ( c == d )   return true ;
    
    return IsWhiteSpace(c) ;
}

string LCP ( const vector<string> & names ) {
    int lmin = INT32_MAX ;
    for ( auto & s : names )
        lmin = min<int>(lmin, s.size()) ;
    
    // Algorithm to find the lowest common prefix
    string com = "" ;
    for ( int i = 0 ; i < lmin ; i ++ ) {
        int allsame = true ;
        char c = names[0][i] ;

        for ( auto & s : names )
            if ( s[i] != c ) {
                allsame = false ;
                break ;
            }
        
        if ( ! allsame )    break ;
        com.push_back(c) ;
    }
    return com ;
}

string AutoComplete ( const char * cmd , int * stat ) {
    int l = strlen(cmd) ;
    int si = l - 1 ;
    while ( si >= 0 && ! IsDelim(cmd[si]) )    si -- ;
    si ++ ;

    // Algorithm to autocomplete the last word entered by matching it with the names of the files and folders
    // If multiple matches, then take the LCP of the multiple matches

    struct dirent * dir ;
    DIR * d = opendir(".") ;
    vector<string> names ;
    if ( d ) {
        while ( (dir = readdir(d)) != NULL ) {
            if ( strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..") )
                names.push_back(string(dir->d_name)) ;
        }
        closedir(d) ;
    }

    vector<string> compls ;
    string cmdstr(cmd) ;
    for ( string & s : names ) {
        if ( l == si || cmdstr.substr(si) == s.substr(0, l-si) )
            compls.push_back(s) ;
    }

    if ( compls.size() == 0 )   { (*stat) = 0 ; return "" ; }
    if ( compls.size() == 1 )   { (*stat) = 0 ; return compls[0].substr(l-si) ;}

    string lcp = LCP(compls) ;
    if ( l - si < lcp.size() ) {
        (*stat) = 0 ;
        return lcp.substr(l-si) ;
    }

    sort(compls.begin(), compls.end()) ;
    (*stat) = 1 ; int sno = 1 ;
    printf("\n (0.)\t[ SKIP ]\n") ;
    for ( string & s : compls ) 
        printf(" (%d.)\t%s\n", sno ++, s.c_str()) ;
    
    int x = ReadNum() ;
    if ( x <= 0 || x > compls.size() )   return "" ;
    return compls[x-1].substr(l-si) ;
}

string ReadCmd ( ) {
    char c ;
    string cmd = "" ;
    int cursorx = CurrentWorkingDir(0) ;
    readstate = true ;
    
    // Keep reading characters until newline character is detected (user presses enter) while taking care of non-printable characters
    while ( (c = getchar()) != '\n' ) {
        if ( ! readstate || c == __ctrl_plus_C__ ) {
            printf(" ERROR : Invalid command.\n") ;
            cmd.clear() ;
            break ;
        }

        if ( c == __tab__ ) {
            int jmp = 8 - cursorx % 8 ;
            if ( ! jmp )    jmp = 8 ;
            Backspace(jmp) ;

            int stat ;
            string cmp = AutoComplete(cmd.c_str(), &stat) ;
            cmd += cmp ;

            if ( stat ) {
                cursorx = CurrentWorkingDir() ;
                readstate = true ;
                printf("%s", cmd.c_str()) ;
                cursorx += cmd.size() ;
            }
            else {
                printf("%s", cmp.c_str()) ;
                cursorx += cmp.size() ;
            }
            continue ;
        }
        
        if ( c == __ctrl_plus_R__ ) {
            cmd = SearchHistory() ;
            CurrentWorkingDir() ;
            printf("%s", cmd.c_str()) ;
            continue ;
        }

        if ( c == __backspace__ ) {
            Backspace(2) ;
            int l = cmd.size() ;
            if ( l > 0 && cmd[l-1] <= __non_printable_ascii_lim__ )  { Backspace(1) ; cursorx -- ; }
            if ( l ) { cmd.pop_back() ; Backspace(1) ; cursorx -- ; }
            continue ;
        }

        if ( c <= __non_printable_ascii_lim__ ) cursorx ++ ;
        cursorx ++ ;
        cmd.push_back(c) ;
    }
    
    readstate = false ;
    if ( HasNonPrintableChar(cmd) ) {
        printf(" ERROR : Invalid command.\n") ;
        return "" ;
    }
    return cmd ;
}

int IsPiped ( const char * cmd ) {
    int l = strlen(cmd) ;

    // Returns true if the command has a pipe character ('|') else false
    for ( int i = 0 ; i < l ; i ++ )
        if ( cmd[i] == '|' )    return true ;
    return false ;
}

char ** ParseNonPipedCmd ( const char * cmd ) {
    if ( ! cmd[0] ) return NULL ;
    
    int l = strlen(cmd) ;
    int i = 0 ;
    vector<char *> argsvec ;

    // Algorithm to vectorize the cmd entered based on space
    // At the same time it checks for pairing of quotes (single and double), and back ticks 
    while ( i < l ) {
        while ( i < l && IsWhiteSpace(cmd[i]) )    i ++ ;
        if ( i == l )   break ;

        int argsize = 0 ;
        int spclcase = 0 ;

        if ( cmd[i] == '"' ) {
            i ++ ;  spclcase = 1 ;
            int esc = false ;
            while ( i < l && (esc || cmd[i] != '"') ) { 
                esc = (cmd[i] == '\\') ;
                argsize ++ ; i ++ ; 
            }
            if ( i == l )   {
                printf(" ERROR : Invalid command.\n") ;
                return NULL ;
            }
        }

        else if ( cmd[i] == '\'' ) {
            i ++ ;  spclcase = 1 ;
            int esc = false ;
            while ( i < l && (esc || cmd[i] != '\'') ) { 
                esc = (cmd[i] == '\\') ;
                argsize ++ ; i ++ ;
            }
            if ( i == l )   {
                printf(" ERROR : Invalid command.\n") ;
                return NULL ;
            }
        }

        else {
            while ( i < l && ! IsWhiteSpace(cmd[i]) ) 
                { argsize ++ ; i ++ ; }
        }

        string arg ;
        char prev = 0 ;
        while ( argsize ) {
            char curr = cmd[i - (argsize --)] ;
            if ( prev == '\\' && (curr == '\'' || curr == '"' || curr == '`') ) 
                arg.pop_back() ;
            prev = curr ;
            arg.push_back(prev) ;
        }
        
        argsvec.push_back(strdup(arg.c_str())) ;
        if ( spclcase )     i ++ ;
    }

    l = argsvec.size() ;
    char ** args = (char **) calloc (l + 1, sizeof(char *)) ;
    for ( int i = 0 ; i < l ; i ++ )
        args[i] = argsvec[i] ;
    args[l] = NULL ;
    
    return args ;
}

vector<char **> ParsePipedCmd ( const char * cmd ) {
    vector<char **> pipedcmds ;
    string subcmd = "" ;
    int charsread = false ;
    int len = strlen(cmd) ;

    int esc = false ;
    stack<string> quotations ;
    
    // Algorithm to vectorize the piped cmd entered based on pipe character (|)
    // At the same time it checks for pairing of quotes (single and double), and back ticks 
    for ( int i = 0 ; i < len ; i ++ ) {
        if ( cmd[i] == '|' && quotations.empty() ) {
            if ( ! charsread ) {
                printf(" ERROR : Invalid command.\n") ;
                for ( char ** args : pipedcmds )    free(args) ;
                return {} ;
            }

            Strip(subcmd) ;
            if ( subcmd.back() == '&' ) {
                printf(" ERROR : No individual commands in the pipe can run in the background.\n") ;
                for ( char ** args : pipedcmds )    free(args) ;
                return {} ;
            }

            pipedcmds.push_back(ParseNonPipedCmd(subcmd.c_str())) ;
            subcmd.clear() ; charsread = false ;
        }

        else {
            if ( ! charsread && IsWhiteSpace(cmd[i]) )  continue ;
            subcmd.push_back(cmd[i]) ;
            charsread = true ;

            if ( cmd[i] == '"' ) {
                if ( esc ) {
                    if ( ! quotations.empty() && quotations.top() == "ESC\"" )
                        quotations.pop() ;
                    else    quotations.push("ESC\"") ;
                }
                else {
                    if ( ! quotations.empty() && quotations.top() == "\"" )
                        quotations.pop() ;
                    else    quotations.push("\"") ;
                }
            }

            if ( cmd[i] == '\'' ) {
                if ( esc ) {
                    if ( ! quotations.empty() && quotations.top() == "ESC'" )
                        quotations.pop() ;
                    else    quotations.push("ESC'") ;
                }
                else {
                    if ( ! quotations.empty() && quotations.top() == "'" )
                        quotations.pop() ;
                    else    quotations.push("'") ;
                }
            }

            if ( cmd[i] == '`' ) {
                if ( esc ) {
                    if ( ! quotations.empty() && quotations.top() == "ESC`" )
                        quotations.pop() ;
                    else    quotations.push("ESC`") ;
                }
                else {
                    if ( ! quotations.empty() && quotations.top() == "`" )
                        quotations.pop() ;
                    else    quotations.push("`") ;
                }
            }

            esc = (cmd[i] == '\\') ;
        }
    }

    if ( ! charsread || ! quotations.empty() ) {
        printf(" ERROR : Invalid command.\n") ;
        for ( char ** args : pipedcmds )    free(args) ;
        return {} ;
    }

    Strip(subcmd) ;
    if ( subcmd.back() == '&' ) {
        printf(" ERROR : No individual commands in the pipe can run in the background.\n") ;
        for ( char ** args : pipedcmds )    free(args) ;
        return {} ;
    }
    
    pipedcmds.push_back(ParseNonPipedCmd(subcmd.c_str())) ;
    return pipedcmds ;
}

string GetCommand ( char ** args ) {
    // Function returns the command name of the command
    string cmd ;
    int iter = 0 ;
    while ( args[iter] ) 
        cmd += string(args[iter++]) + " " ;
    if ( cmd.size() )   cmd.pop_back() ;
    return cmd ;
}

int ExecPipedCmd ( char ** args , int readfd , int writefd ) {
    if ( ! args || ! args[0] )   return 0 ;

    int pid = fork() ;
	if ( pid == -1 ) {
		printf(" ERROR : Child process not successfully created.\n") ;
		return -1 ;
	}

    if ( pid != 0 ) {
        int status = 0 ;
        child_pid = pid ;
        int retid = -1 ;
        while ( retid != pid )
            retid = wait(&status) ;
        return status ;
    }

    int iter = 0 ;
    string input = "" ;
    string output = "" ;
    int lastargi = -2 ;

    int infilegiven = false ;
    int outfilegiven = false ;

    // Executes the one of the commands of a piped command using the correct read and write files (usually an end of a pipe)
    while ( args[iter] ) {
        if ( strcmp(args[iter], "<") == 0 ) {
            if ( ! args[iter + 1] ) {
                printf(" ERROR : Input file not given.\n") ;
                free(args) ;    exit(-1) ;
            }
            else if ( infilegiven ) {
                printf(" ERROR : Multiple input files given.\n") ;
                free(args) ;    exit(-1) ;
            }
            infilegiven = true ;
            if ( lastargi == -2 )   lastargi = iter - 1 ;
            input = string(args[iter + 1]) ;
        }

        else if ( strcmp(args[iter], ">") == 0 ) {
            if ( ! args[iter + 1] ) {
                printf(" ERROR : Output file not given.\n") ;
                free(args) ;    exit(-1) ;
            }
            else if ( outfilegiven ) {
                printf(" ERROR : Multiple output files given.\n") ;
                free(args) ;    exit(-1) ;
            }
            outfilegiven = true ;
            if ( lastargi == -2 )   lastargi = iter - 1 ;
            output = string(args[iter + 1]) ;
        }

        iter ++ ;
    }

    if ( lastargi == -1 && strcmp(args[0], "<") == 0  ) {
        printf(" ERROR : Invalid command.\n") ;
        free(args) ;    exit(-1) ;
    }
    if ( lastargi >= -1 )    args[lastargi + 1] = NULL ;

    int inputfd = -1 , outputfd = -1 ;
    int stdout_copy = -1 ;

    if ( input.size() ) {
        close(0) ;
        inputfd = open(input.c_str(), O_RDONLY) ;
        if ( inputfd < 0 ) {
            printf(" ERROR : Input file not found.\n") ;
            free(args) ;    exit(-1) ;
        }
        dup(inputfd) ;
    }
    else if ( readfd != 0 ) {
        close(0) ;
        dup(readfd) ;
    }

    if ( output.size() ) {
        stdout_copy = dup(1) ;
        close(1) ;
        outputfd = open(output.c_str(),  O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU) ;
        if ( outputfd < 0 ) {
            printf(" ERROR : Output file could not be found/created.\n") ;
            if ( inputfd > 0 ) { close(inputfd) ; }
            free(args) ;    exit(-1) ;
        }
        dup(outputfd) ;
    }
    else if ( writefd != 1 ) {
        stdout_copy = dup(1) ;
        close(1) ;
        dup(writefd) ;
    }

    if ( args[0] && ! args[1] && strcmp(args[0], "history") == 0 ) {
        PrintHistory(__hist_print_size__) ;
    }

    else if ( args[0] && strcmp(args[0], "getbackg") == 0 ) {
        if ( args[1] && ! args[2] && strcmp(args[1], "--old") == 0 ) {
            PrintKilledBackgroundProcs() ;
        }
        else if ( ! args[1] ) {
            PrintRunningBackgroundProcs() ;
        }
        else {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 ) { 
                close(outputfd) ;
                dup2(stdout_copy, 1) ; 
                close(stdout_copy) ;
            }
            printf(" ERROR : Invalid command.\n") ;
            free(args) ;
            exit(-1) ;
        }
    }

    else if ( args[0] ) {
        if ( execl("/bin/sh", "sh", "-c", GetCommand(args).c_str(), (char*)NULL) < 0 ) {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 ) { 
                close(outputfd) ;
                dup2(stdout_copy, 1) ; 
                close(stdout_copy) ;
            }
            printf(" ERROR : Invalid command.\n") ;
            free(args) ;
            exit(-1) ;
        }
    }

    if ( inputfd > 0 ) { close(inputfd) ; }
    if ( outputfd > 0 ) { close(outputfd) ; }
    free(args) ;
    exit(0) ;
}

int ExecPipedCmd ( vector<char **> & argsvec , int default_outfile = 1 ) {
    if ( ! argsvec.size() || ! argsvec[0] )   return 0 ;

    int pipefd [2] ;
    int readfd = 0 ;
    int sno = 1 ;
    int len = argsvec.size() ;

    // Sets up a pipe messaging system and executes the component commands of the piped command one by one using one end of the pipe to read the output of the previous command and writing to the other end of the pipe
    for ( char ** args : argsvec ) {
        if ( sno < len ) {
            if ( pipe(pipefd) < 0 ) {
                printf(" ERROR : Pipe could not be created.\n") ;
                if ( readfd != 0 )  close(readfd) ;
		        return -1 ;
            }
        }
        else    pipefd[1] = default_outfile ;

        int status = ExecPipedCmd(args, readfd, pipefd[1]) ;
        if ( readfd != 0 )  close(readfd) ;
        if ( pipefd[1] != 1 )  close(pipefd[1]) ;

        readfd = pipefd[0] ;
        sno ++ ;
        if ( status < 0 )   return -1 ;
    }

    return 0 ;
}

int ExecNonPipedCmd ( char ** args , int default_outfile = 1 , const string & cmdorg = string("") ) {
    long start_time = time(NULL) ;
    if ( ! args || ! args[0] )   return 0 ;

    if ( strcmp(args[0], "cd") == 0 ) {
        chdir(args[1]) ;
        return 0 ;
    }

    int runbackg = false ;
    int iter = 0 ;
    while ( args[iter + 1] )    iter ++ ;
    if ( strcmp(args[iter], "&") == 0 ) runbackg = 1 ;

    string bgoutf = bg_procs_default_outfile + "_" + to_string(bgid) + ".myshell.out" ;

    int pid = fork() ;
	if ( pid == -1 ) {
		printf(" ERROR : Child process not successfully created.\n") ;
		return -1 ;
	}

    if ( pid != 0 ) {
        if ( ! runbackg ) {
            child_pid = pid ; 
            int retid = -1 ;
            while ( retid != pid )
                retid = wait(NULL) ;
            return 0 ;
        }

        bgid ++ ;
        all_backg_procs.insert({pid, {start_time, bgid-1, cmdorg}}) ;
        printf(" [ The command is being executed in the background ]\n") ;
        printf(" INFO : Output (if directed to STDOUT stream) will be re-directed \n to \"%s\" file to maintain the current \n user interactive experience.\n", bgoutf.c_str()) ;
        return 0 ;
    }

    if ( default_outfile != 1 ) {
        close(1) ;
        dup(default_outfile) ;
    }

    int len = 0 ;
    while ( args[len] ) len ++ ;

    if ( runbackg ) args[iter] = NULL ;
    if ( ! args[0] )    exit(0) ;

    iter = 0 ;
    string input = "" ;
    string output = "" ;
    int lastargi = -2 ;

    int infilegiven = false ;
    int outfilegiven = false ;

    // Executes the command while taking care of redirection operators and input output streams
    // We also introduce some new commands to keep track of background process whose output is stored in a file
    while ( args[iter] ) {
        if ( strcmp(args[iter], "<") == 0 ) {
            if ( ! args[iter + 1] ) {
                printf(" ERROR : Input file not given.\n") ;
                free(args) ;    exit(-1) ;
            }
            else if ( infilegiven ) {
                printf(" ERROR : Multiple input files given.\n") ;
                free(args) ;    exit(-1) ;
            }
            infilegiven = true ;
            if ( lastargi == -2 )   lastargi = iter - 1 ;
            input = string(args[iter + 1]) ;
        }

        else if ( strcmp(args[iter], ">") == 0 ) {
            if ( ! args[iter + 1] ) {
                printf(" ERROR : Output file not given.\n") ;
                free(args) ;    exit(-1) ;
            }
            else if ( outfilegiven ) {
                printf(" ERROR : Multiple output files given.\n") ;
                free(args) ;    exit(-1) ;
            }
            outfilegiven = true ;
            if ( lastargi == -2 )   lastargi = iter - 1 ;
            output = string(args[iter + 1]) ;
        }

        iter ++ ;
    }

    if ( lastargi == -1 && strcmp(args[0], "<") == 0  ) {
        printf(" ERROR : Invalid command.\n") ;
        free(args) ;    exit(-1) ;
    }
    if ( lastargi >= -1 )    args[lastargi + 1] = NULL ;

    int inputfd = -1 , outputfd = -1 ;
    int stdout_copy = -1 ;

    if ( input.size() ) {
        close(0) ;
        inputfd = open(input.c_str(), O_RDONLY) ;
        if ( inputfd < 0 ) {
            printf(" ERROR : Input file not found.\n") ;
            free(args) ;    exit(-1) ;
        }
        dup(inputfd) ;
    }

    if ( runbackg && ! output.size() ) 
        output = bgoutf ;

    if ( output.size() ) {
        stdout_copy = dup(1) ;
        close(1) ;
        outputfd = open(output.c_str(),  O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU) ;
        if ( outputfd < 0 ) {
            printf(" ERROR : Output file could not be found/created.\n") ;
            if ( inputfd > 0 ) { close(inputfd) ; }
            free(args) ;    exit(-1) ;
        }
        dup(outputfd) ;
    }

    if ( runbackg ) {
        close(2) ;
        dup(outputfd) ;
    }

    if ( args[0] && ! args[1] && strcmp(args[0], "history") == 0 ) {
        if ( runbackg ) {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 && ! runbackg ) {
                close(outputfd) ;
                dup2(stdout_copy, default_outfile) ;
                close(stdout_copy) ;
            }
            printf(" ERROR : \"history\" command cannot be run in the background.\n") ;
            free(args) ;
            if ( default_outfile != 1 ) close(default_outfile) ;
            exit(-1) ;
        }
        PrintHistory(__hist_print_size__) ;
    }

    else if ( args[0] && strcmp(args[0], "getbackg") == 0 ) {
        if ( runbackg ) {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 && ! runbackg ) {
                close(outputfd) ;
                dup2(stdout_copy, default_outfile) ;
                close(stdout_copy) ;
            }
            printf(" ERROR : \"getbackg\" command cannot be run in the background.\n") ;
            free(args) ;
            if ( default_outfile != 1 ) close(default_outfile) ;
            exit(-1) ;
        }
        if ( args[1] && ! args[2] && strcmp(args[1], "--old") == 0 ) {
            PrintKilledBackgroundProcs() ;
        }
        else if ( args[1] && ! args[2] && strcmp(args[1], "--prevmode") == 0 ) {
            PrintKilledBackgroundProcs(1) ;
        }
        else if ( ! args[1] ) {
            PrintRunningBackgroundProcs() ;
        }
        else {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 ) { 
                close(outputfd) ;
                dup2(stdout_copy, default_outfile) ;
                close(stdout_copy) ;
            }
            printf(" ERROR : Invalid command.\n") ;
            free(args) ;
            if ( default_outfile != 1 ) close(default_outfile) ;
            exit(-1) ;
        }
    }

    else if ( args[0] ) {
        if ( execl("/bin/sh", "sh", "-c", GetCommand(args).c_str(), (char*)NULL) < 0 ) {
            if ( inputfd > 0 ) { close(inputfd) ; }
            if ( outputfd > 0 && ! runbackg ) {
                close(outputfd) ;
                dup2(stdout_copy, default_outfile) ;
                close(stdout_copy) ;
            }
            printf(" ERROR : Invalid command.\n") ;
            free(args) ;
            if ( default_outfile != 1 ) close(default_outfile) ;
            exit(-1) ;
        }
    }

    if ( inputfd > 0 ) { close(inputfd) ; }
    if ( outputfd > 0 ) { close(outputfd) ; }
    free(args) ;
    exit(0) ;
}

int UpdateBashHistory ( ) {
    int histfd = open(".bash_history.cache", O_RDWR|O_TRUNC) ;
    if ( histfd < 0 ) {
        printf(" ERROR : Bash history could not be updated.\n") ;
		return -1 ;
    }

    int stdout_copy = dup(1) ;
    close(1) ;
    dup(histfd) ;

    // Print the history of commands into the file
    for ( string & cmd : history_of_cmds ) 
        printf("%s\n", cmd.c_str()) ;
    
    close(histfd) ;
    dup2(stdout_copy, 1) ;
    close(stdout_copy) ;
    return 0 ;
}

void Stop ( int x ) {
    // If there is an active child process, kill the process using the KILL SIGNAL
    if ( child_pid > 0 )
        kill(child_pid, SIGKILL) ;
    
    // For all the processes of the mutiwatch command, kill them all using the KILL SIGNAL
    for ( int pid : multiwatch_child_pids ) {
        kill(pid, SIGKILL) ;
        string fifoname = ".temp." + to_string(pid) + ".txt" ;
        remove(fifoname.c_str()) ;
    }
    
    multiwatch_child_pids.clear() ;
    child_pid = 0 ;

    if ( ! readstate )  printf("\n") ;
    readstate = false ;
}

void Exit ( int x ) {
    UpdateBashHistory() ;
    
    // If there are active multiwatch children processes KILL ALL PROCESSES AND REMOVE TEMPORARY FILES
    if ( multiwatch_child_pids.size() ) {
        for ( int pid : multiwatch_child_pids ) {
            kill(pid, SIGKILL) ;
            string fifoname = ".temp." + to_string(pid) + ".txt" ;
            remove(fifoname.c_str()) ;
        }

        if ( child_pid > 0 )
            kill(child_pid, SIGKILL) ;
    }

    // Print the killed background processes
    char ** args = ParseNonPipedCmd("getbackg --prevmode") ;
    ExecNonPipedCmd(args, 1, "getbackg --prevmode") ;
    if ( args ) free(args) ;
    
    if ( x == SIGSEGV )
        sigset(SIGSEGV, default_SIGSEGV_handler) ;
    else 
        sigset(SIGTSTP, default_SIGTSTP_handler) ;
    raise(x) ;
}

void RefineMultiWatchCmds ( vector<string> & args ) {
    if ( ! args.size() ) {
        printf(" ERROR : Invalid command.\n") ;
        return ;
    }

    if ( args[0] == "[]" ) {
        args.clear() ;
        return ;
    }

    // Refines the multiwatch commands taking care of opening and ending square brackets and double quotes
    // Therefore modifying the vector of multiwatch commands
    int len = args.size() ;
    if ( len == 1 ) {
        int l = args[0].size() ;
        if ( l <= 4 || args[0][0] != '[' || args[0][1] != '"' 
                    || args[0][l-1] != ']' || args[0][l-2] != '"' ) {
            printf(" ERROR : Invalid command.\n") ;
            args.clear() ;  return ;
        }
        args[0] = args[0].substr(2, l-4) ;
        return ;
    }

    int l = args[0].size() ;
    if ( l <= 2 || args[0][0] != '[' || args[0][1] != '"' ) {
        printf(" ERROR : Invalid command.\n") ;
        args.clear() ;  return ;
    }
    args[0] = args[0].substr(2, l-2) ;

    for ( int i = 1 ; i < len - 1 ; i ++ ) {
        if ( ! args[i].size() ) {
            printf(" ERROR : Invalid command.\n") ;
            args.clear() ;  return ;
        }
    }

    l = args[len-1].size() ;
    if ( l <= 2 || args[len-1][l-1] != ']' || args[len-1][l-2] != '"' ) {
        printf(" ERROR : Invalid command.\n") ;
        args.clear() ;  return ;
    }
    args[len-1] = args[len-1].substr(0, l-2) ;
    return ;
}

int ExecMultiWatchCmd ( const char * cmd , int tempfd ) {
    // Simply executes a component command of the multiwatch watch command and redirects its output to the correct file descriptor
    if ( ! IsPiped(cmd) ) {
        char ** args = ParseNonPipedCmd(cmd) ;
        int ret = ExecNonPipedCmd(args, tempfd) ;
        close(tempfd) ;
        if ( args ) free(args) ;
        return ret ;
    }

    else {
        auto argsvec = ParsePipedCmd(cmd) ;
        int ret = ExecPipedCmd(argsvec, tempfd) ;
        close(tempfd) ;
        for ( char ** args : argsvec )  free(args) ;
        return ret ;
    }
}

int IsMultiWatchCmd ( char * & cmd ) {
    int l = strlen(cmd) ;
    if ( l < 13 || strncmp(cmd, "multiWatch ", 11) ) return false ;

    // Checks whether the command is a multiwatch command in the correct format
    string modcmd = "multiWatch " ;
    int i = 11 ;
    int inside = 0 ;
    while ( i < l ) {
        if ( cmd[i] == '"' )    {
            modcmd.push_back(cmd[i]) ;
            inside = ! inside ;
        }
        else if ( ! inside ) {
            char b = modcmd.back() ;
            if ( cmd[i] != ' ' || (b != ' ' && b != '[' && b != '"') )
                modcmd.push_back(cmd[i]) ;
        }
        else    modcmd.push_back(cmd[i]) ;
        i ++ ;
    }

    cmd = strdup(modcmd.c_str()) ;
    return true ;
}

int ParseFileName ( string & filename ) {
    // Program to check whether the file name is valid (Checking of matching double quotes and single quotes)
    if ( ! filename.size() )    return 0 ;
    if ( filename[0] == '\'' || filename[0] == '"' ) {
        int i = 1 ;
        int l = filename.size() ;
        if ( l == 2 )   return 0 ;

        while ( i < l && filename[i] != filename[0] )   i ++ ;
        if ( i != l - 1 )   return 0 ;

        if ( l == 2 )   return 0 ;
        filename = filename.substr(1, l-2) ;
        return 1 ;
    }

    int i = 0 ;
    int l = filename.size() ;

    while ( i < l && filename[i] != ' ' )   i ++ ;
    if ( i < l )   return 0 ;
    return 1 ;
}

vector<string> ParseMultiWatchCmd ( const char * cmd ) {
    // Parses the Multiwatch command and return the vectorized form of the multiwatch command and set the redirecting outfile for multiwatch
    
    multiwatch_redirect_outfile.clear() ;
    int l = strlen(cmd) ;

    if ( cmd[l-1] != ']' ) {
        int i = l - 1 ;
        string suffix ;
        while ( i >= 0 && cmd[i] != ']' ) {
            suffix.push_back(cmd[i]) ;
            i -- ;
        }

        if ( i < 0 ) {
            printf(" ERROR : Invalid command.\n") ;
            return { } ;
        }

        reverse(suffix.begin(), suffix.end()) ;
        Strip(suffix) ;

        if ( ! suffix.size() || suffix[0] != '>' ) {
            printf(" ERROR : Invalid command.\n") ;
            return { } ;
        }
        
        multiwatch_redirect_outfile = suffix.substr(1) ;
        Strip(multiwatch_redirect_outfile) ;

        if ( ! ParseFileName(multiwatch_redirect_outfile) ) {
            multiwatch_redirect_outfile.clear() ;
            printf(" ERROR : Invalid command.\n") ;
            return { } ;
        }

        l = i + 1 ;
    }

    vector<string> argsvec ;
    int i = 11 ;
    string subcmd = "" ;

    while ( i < l ) {
        if ( i + 3 < l && cmd[i] == '"' && cmd[i+1] == ',' && cmd[i+2] == ' ' && cmd[i+3] == '"' ) {
            argsvec.push_back(subcmd) ;
            subcmd.clear() ;    i += 4 ;
        }
        else    subcmd.push_back(cmd[i ++]) ;
    }
    argsvec.push_back(subcmd) ;
    return argsvec ;
}

int MultiWatch ( char * longcmd ) {
    // Executes the multiwatch commands one by one and waits for all child processes to complete
    vector<string> args = ParseMultiWatchCmd(longcmd) ;
    if ( ! args.size() )  {
        multiwatch_redirect_outfile.clear() ;
        return 0 ;
    }

    RefineMultiWatchCmds(args) ;
    unordered_map<int, string> child_2_cmd ;
    unordered_set<int> child_pids_set ;

    for ( string & cmdstr : args ) {
        Strip(cmdstr) ;

        if ( cmdstr.back() == '&' ) {
            printf(" ERROR : All commands in \"multiWatch\" should run in the foreground.\n") ;
            for ( int child : multiwatch_child_pids ) {
                kill(child, SIGKILL) ;
                string fifoname = ".temp." + to_string(child) + ".txt" ;
                remove(fifoname.c_str()) ;
            }
            multiwatch_child_pids.clear() ;
            multiwatch_redirect_outfile.clear() ;
            return -1 ;
        }

        const char * cmd = cmdstr.c_str() ;
        
        int pid = fork() ;

        if ( pid < 0 ) {
            printf(" ERROR : Child process not successfully created.\n") ;
            for ( int child : multiwatch_child_pids ) {
                kill(child, SIGKILL) ;
                string fifoname = ".temp." + to_string(child) + ".txt" ;
                remove(fifoname.c_str()) ;
            }
            multiwatch_child_pids.clear() ;
            multiwatch_redirect_outfile.clear() ;
            return -1 ;
        }

        if ( pid == 0 ) {
            int thispid = getpid() ;
            string fifoname = ".temp." + to_string(thispid) + ".txt" ;
            
            int fifo = mkfifo(fifoname.c_str(), S_IRWXU) ;
            if ( fifo < 0 && errno != EEXIST ) {
                printf(" ERROR [CHILD PID %d] : Named pipe \"%s\" could not be created successfully.\n", thispid, fifoname.c_str()) ;
                exit(-1) ;
            }

            int tempfd = open(fifoname.c_str(), O_WRONLY) ;
            int ret = ExecMultiWatchCmd(cmd, tempfd) ;
            exit(ret) ;
        }

        string fifoname = ".temp." + to_string(pid) + ".txt" ;
        int fifo = mkfifo(fifoname.c_str(), S_IRWXU) ;

        if ( fifo < 0 && errno != EEXIST ) {
            printf(" ERROR : Named pipe \"%s\" could not be created successfully.\n", fifoname.c_str()) ;
            for ( int child : multiwatch_child_pids ) {
                kill(child, SIGKILL) ;
                string fifoname = ".temp." + to_string(child) + ".txt" ;
                remove(fifoname.c_str()) ;
            }
            multiwatch_child_pids.clear() ;
            multiwatch_redirect_outfile.clear() ;
            return -1 ;
        }

        multiwatch_child_pids.push_back(pid) ;
        child_pids_set.insert(pid) ;
        child_2_cmd.insert({pid, cmdstr}) ;
    }

    int pid = fork() ;
    if ( pid < 0 ) {
        printf(" ERROR : Child process not successfully created.\n") ;
        for ( int child : multiwatch_child_pids ) {
            kill(child, SIGKILL) ;
            string fifoname = ".temp." + to_string(child) + ".txt" ;
            remove(fifoname.c_str()) ;
        }
        multiwatch_child_pids.clear() ;
        multiwatch_redirect_outfile.clear() ;
        return -1 ;
    }

    if ( pid > 0 ) {
        child_pid = pid ;
        
        while ( child_pids_set.size() ) {
            int x = wait(NULL) ;
            if ( child_pids_set.find(x) != child_pids_set.end() )
                child_pids_set.erase(x) ;
        }

        wait(NULL) ;

        for ( int child : multiwatch_child_pids ) {
            string fifoname = ".temp." + to_string(child) + ".txt" ;
            remove(fifoname.c_str()) ;
        }

        multiwatch_child_pids.clear() ;
        multiwatch_redirect_outfile.clear() ;
        return 0 ;
    }

    int stdout_copy ;
    int outputfd = -1 ;
    if ( multiwatch_redirect_outfile.size() ) {
        stdout_copy = dup(1) ;
        close(1) ;
        outputfd = open(multiwatch_redirect_outfile.c_str(),  O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU) ;
        if ( outputfd < 0 ) {
            printf(" ERROR : Redirected output file \"%s\" could not be found/created.\n", 
                    multiwatch_redirect_outfile.c_str()) ;
            exit(-1) ;
        }
        dup(outputfd) ;
        multiwatch_redirect_outfile.clear() ;
    }

    fd_set readfds ;
    struct timeval tv ;
    char buf [101] ;
    unordered_map<int, int> fd_2_child ;

    unordered_set<int> tempfds ;
    int maxreadfd = -1 ;
    for ( int id : multiwatch_child_pids ) {
        string name = ".temp." + to_string(id) + ".txt" ;
        int tempfd = open(name.c_str(), O_RDONLY|O_NONBLOCK) ;

        if ( tempfd < 0 )   continue ;
        tempfds.insert(tempfd) ;
        
        maxreadfd = max<int>(maxreadfd, tempfd) ;
        fd_2_child.insert({tempfd, id}) ;
    }

    while ( true ) {
        FD_ZERO(&readfds) ;
        for ( int fd : tempfds )    FD_SET(fd, &readfds) ;

        tv.tv_sec = __wait_time__ ;
		tv.tv_usec = 0 ;

        int rec = select(maxreadfd + 1, &readfds, NULL, NULL, &tv) ;

        if ( rec < 0 ) {
            for ( int fd : tempfds )    close(fd) ;
            printf(" ERROR : select() gave an unexpected error.\n") ;
            close(outputfd) ;
            dup2(stdout_copy, 1) ;
            close(stdout_copy) ;
            exit(-1) ;
        }

        if ( rec == 0 ) {
            int allended = true ;
            for ( int chpid : multiwatch_child_pids ) {
                if ( kill(chpid, 0) == 0 ) {
                    allended = false ;
                    break ;
                }
            }
            if ( allended ) {
                for ( int fd : tempfds )    close(fd) ;
                if ( __print_timeout__ )
                    printf("\n [ TIMEOUT : Select() could not find a ready file descriptor for too long (%d secs) ]\n", __wait_time__) ;
                close(outputfd) ;
                dup2(stdout_copy, 1) ;
                close(stdout_copy) ;
                exit(0) ;
            }
            continue ;
        }

        vector<int> finished ;
        for ( int fd : tempfds ) {
            if ( ! rec )    break ;
            if ( ! FD_ISSET(fd, &readfds) ) continue ;

            int bytes = read(fd, buf, 100) ;
            if ( bytes < 0 )   continue ;
            if ( bytes == 0 ) {
                finished.push_back(fd) ;
                continue ;
            }
            
            printf("\n \"%s\" , CURRENT TIME : %ld\n", child_2_cmd.at(fd_2_child.at(fd)).c_str(), time(NULL)) ;
            printf("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n") ;
            
            buf[bytes] = 0 ;
            printf("%s", buf) ;
            while ( (bytes = read(fd, buf, 100)) > 0 ) {
                buf[bytes] = 0 ;
                printf("%s", buf) ;
            }
            
            printf("->->->->->->->->->->->->->->->->->->->\n") ;
            rec -- ;
        }

        for ( int fd : finished )
            tempfds.erase(fd) ;
    }

    exit(0) ;
}

void VoluntaryExit ( ) {
    raise(SIGTSTP) ;
}

void SetKeyboard ( ) {
    // The termios functions describe a general terminal interface that is provided to control asynchronous communications ports.

    // tcgetattr() gets the parameters associated with the object referred by fd and stores them in the termios structure referenced by termios_p. This function may be invoked from a background process; however, the terminal attributes may be subsequently changed by a foreground process.
    // tcsetattr() sets the parameters associated with the terminal (unless support is required from the underlying hardware that is not available) from the termios structure referred to by termios_p. optional_actions specifies when the changes take effect

    tcgetattr(0, &oldtio) ;
    memcpy(&newtio, &oldtio, sizeof(struct termios)) ;

    // The setting of the ICANON canon flag in c_lflag determines
    //    whether the terminal is operating in canonical mode (ICANON set)
    //    or noncanonical mode (ICANON unset).  By default, ICANON is set.


    // In noncanonical mode input is available immediately (without the
    //    user having to type a line-delimiter character), no input
    //    processing is performed, and line editing is disabled.  The read
    //    buffer will only accept 4095 chars; this provides the necessary
    //    space for a newline char if the input mode is switched to
    //    canonical. 
    newtio.c_lflag &= ~(ICANON) ;

    // The point of these is that if you're writing to a serial terminal, it may take time for data written to be flushed. The different values ensure that change occurs when you want it to.
    // The TCSASOFT one is custom to BSD and Linux. You can see from the manual page you quote:
    // TCSANOW — Make the change immediately.
    // The terminal is set to new attributes
    // If optional_actions is TCSANOW, the change shall occur immediately.
    tcsetattr(0, TCSANOW, &newtio) ;
}

void ResetKeyboard ( ) {
    tcsetattr(0, TCSANOW, &oldtio) ;
}

void Home ( ) {
    // Initialises the Bash History and Records the user's history of commands as we keep on executing the commands

    Init() ;
    while ( true ) {
        CurrentWorkingDir() ;
        SetKeyboard() ;
        string cmdstr = ReadCmd() ;
        Strip(cmdstr) ;
        char * cmd = strdup(cmdstr.c_str()) ;
        ResetKeyboard() ;

        if ( ! strlen(cmd) ) continue ;
        
        if ( ! strcmp(cmd, "exit") || ! strcmp(cmd, "quit") ) 
            VoluntaryExit() ;

        if ( history_of_cmds_size == __hist_size__ ) {
            string & lastcmd = history_of_cmds.back() ;
            history_of_cmds_unq.at(lastcmd) -- ;
            if ( ! history_of_cmds_unq.at(lastcmd) )
                history_of_cmds_unq.erase(lastcmd) ;
            history_of_cmds.pop_back() ;
            history_of_cmds_size -- ;
        }

        history_of_cmds.push_front(cmdstr) ;
        history_of_cmds_size ++ ;

        if ( history_of_cmds_unq.find(cmdstr) == history_of_cmds_unq.end() )
            history_of_cmds_unq.insert({cmdstr, 1}) ;
        else    history_of_cmds_unq.at(cmdstr) ++ ;

        if ( IsMultiWatchCmd(cmd) ) {
            MultiWatch(cmd) ;
        }

        else if ( ! IsPiped(cmd) ) {
            char ** args = ParseNonPipedCmd(cmd) ;
            ExecNonPipedCmd(args, 1, cmdstr) ;
            if ( args ) free(args) ;
        }

        else {
            auto argsvec = ParsePipedCmd(cmd) ;
            ExecPipedCmd(argsvec) ;
            for ( char ** args : argsvec )  free(args) ;
        }
    }
}

int main ( int argc , char ** argv ) {
    // SIGINT is the signal sent when we press Ctrl+C. The default action is to terminate the process. However, some programs override this action and handle it differently.    
    // If Ctrl+C is pressed then perform Stop operation
    signal(SIGINT, Stop) ;

    // SIGTSTP is the signal sent when we press Ctrl+Z. 
    // If Ctrl+Z is pressed then perform Exit operation.
    default_SIGTSTP_handler = signal(SIGTSTP, Exit) ;
    if ( default_SIGTSTP_handler == SIG_ERR )   return -1 ;

    // SIGSEGV, also known as a segmentation violation or segmentation fault, 
    // is a signal used by Unix-based operating systems (such as Linux). 
    // It indicates an attempt by a program to write or read outside its allocated memory—either 
    // because of a programming error, a software or hardware compatibility issue, 
    // or a malicious attack, such as buffer overflow.

    default_SIGSEGV_handler = signal(SIGSEGV, Exit) ;
    if ( default_SIGSEGV_handler == SIG_ERR )   return -1 ;
    
    // Initiate the Home function
    Home() ;
    return 0 ;
}
