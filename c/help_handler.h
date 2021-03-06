/* MIT License
 *
 * Copyright (c) 2021 Inaff
 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef HELP_HANDLER_H
#define HELP_HANDLER_H

/* * * * * * * * * * * * * * * * * * * */
/* * * * USER MACROS & VARIABLES * * * */
/* * * * * * * * * * * * * * * * * * * */
/* 
 * These should be the only relevant variables to any library user not trying to change functionality
 */

#define HELP_HANDLER_NO_ARGS true
#define HELP_HANDLER_UNK_ARGS true
#define HELP_HANDLER_UNKNOWN_ARGS true
#define HELP_HANDLER_EXTRA_STRINGS true
#define HELP_HANDLER_DISABLE_NO_ARGS false
#define HELP_HANDLER_DISABLE_UNK_ARGS false
#define HELP_HANDLER_DISABLE_UNKNOWN_ARGS false
#define HELP_HANDLER_DISABLE_EXTRA_STRINGS false

//Return values
static const int helpHandlerSuccess = 0; //This should remain 0, as it's also used to indicate no arguments were matched
static const int helpHandlerFailure = -1;




/* * * * * * * * * * * * * * * */
/* * * * INTERNAL MACROS * * * */
/* * * * * * * * * * * * * * * */
#ifndef __cplusplus //C++ does not support the _Generic macro
#ifdef __STDC__
#ifdef __STDC_VERSION__
#if (__STDC_VERSION__ >= 201112L)
#define HELP_HANDLER_OVERLOAD_SUPPORTED
#endif
#endif
#endif
#endif

#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED       //(0,help) makes array decay to a pointer, otherwise _Generic will throw selection errors (not sure if this is only an issue with Clang, but GCC *does* throw an unused-value warning)
#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif
#define help_handler(argc, argv, help) _Generic((0,help), \
                                const char*: help_handler_s, \
                                const wchar_t*: help_handler_w, \
                                wchar_t*: help_handler_w, \
                                default: help_handler_s) \
                        (argc, argv, help)
#define help_handler_version(ver) _Generic(ver, \
                                const char*: help_handler_version_s, \
                                int: help_handler_version_i, \
                                double: help_handler_version_d, \
                                default: help_handler_version_s) \
                        (ver)
#define help_handler_info(app_name, ver) _Generic(ver, \
                                const char*: help_handler_info_s, \
                                int: help_handler_info_i, \
                                double: help_handler_info_d, \
                                default: help_handler_info_s) \
                        (app_name, ver)
#define help_handler_name(app_name) _Generic(app_name, \
                                const char*: help_handler_name_s, \
                                const wchar_t*: help_handler_name_w, \
                                default: help_handler_name_s) \
                        (app_name)
#define help_handler_pipe(outputPipe) _Generic(outputPipe, \
                                int: help_handler_pipe_i, \
                                default: help_handler_pipe_s) \
                        (outputPipe)
#endif


#include <errno.h>
#include <ctype.h> //For isspace()
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> //strcasecmp causes an implicit function declaration warning in Ubuntu 18.04, but I don't think it does in MacOS
#include <limits.h> //For CHAR_BIT and INT_MIN/INT_MAX
#include <stdbool.h>

//unistd.h and regex.h are OS-specific headers, so check for them on an opt-in basis
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))) //POSIX and Unix
    #include <unistd.h>
    #if defined(_POSIX_VERSION) //POSIX compliant
        #define HELP_HANDLER_POSIX_C
        #include <regex.h>
    #endif
#elif defined(__CYGWIN__) && !defined(_WIN32) //Windows with Cygwin (POSIX)
    #define HELP_HANDLER_POSIX_C
    #include <regex.h>
#elif defined(_WIN64) || defined(_WIN32) //Windows
#include <windows.h>
#else
    //MSVC is the only common compiler that doesn't support the warning directive
    #if _MSC_VER && !__INTEL_COMPILER //Prefixing "warning:" makes MSVC output a warning instead of a message
        #pragma message("warning: Compiling without regular expression support") 
    #else
        #warning "Compiling without regular expression support"
    #endif
#endif


#define MAX_STRING_LEN 64
#define MAX_STRING_COUNT 32


static bool   printErr = true;
static size_t errCount = 0; //Used for counting & printing ALL errors when user calls help_handler_print_err/get_err
static char   errs[MAX_STRING_COUNT][MAX_STRING_LEN] = {{0}};

static const char* helpHandlerFuncName = "help_handler";
static int outputPipe = 0; //Used by print_pipe()

/*
 * These enums below are used for verbosity/changeability internally, in place of what'd otherwise be magic numbers
 */
//C99 and above allows trailing commas
enum output { 
    outDefault = 0, //stdout
    outStdout,
    outStderr, };
enum varTypes {
    nameChar = 0,
    nameWChar,
    versionStr,
    versionInt,
    versionDouble, };
enum returnVal {
    dialogHelpVer = 0,
    dialogHelp,
    dialogVer, };
enum errTypes {
    silent = 0,
    warning,
    error, };

//User info structs
static struct most_recent_t {
    unsigned int name;
    unsigned int ver;
} most_recent_t = { 0, versionStr };

static struct info_t {
    char name[512];
    wchar_t name_w[512];
    char ver_str[512];
    unsigned int ver_int;
    double ver_double;
} info_t = { {0}, {0}, "No version is available", 0, 0 };

static struct options_t {
    bool no_arg_help;
    bool extra_strings;
    bool unknown_arg_help;
} options_t = { true, true, false }; 


bool help_handler_is_err(int errorCode); //Forward declaration so private functions can use this to check for errors




/***************************/
/*                         */
/**** PRIVATE FUNCTIONS ****/
/*                         */
/***************************/
/*
 * String functions
 */
//While making print_pipe take varargs would obviate calling print_pipe repeatedly, it would also cause string-nonliteral warnings (surpressing these would require non-portable macros)
static void print_pipe(const char* s) {
    if (outputPipe == outDefault) {
        fprintf(stdout, "%s", s);
        fflush(stdout);
    } else if (outputPipe == outStdout) {
        fprintf(stdout, "%s", s);
        fflush(stdout);
    } else if (outputPipe == outStderr) {
        fprintf(stderr, "%s", s);
        fflush(stderr); //stderr isn't buffered by default, but flush just in case
    }
}
static void print_pipe_i(int n) {
    if (outputPipe == outDefault) {
        fprintf(stdout, "%d", n);
        fflush(stdout);
    } else if (outputPipe == outStdout) {
        fprintf(stdout, "%d", n);
        fflush(stdout);
    } else if (outputPipe == outStderr) {
        fprintf(stderr, "%d", n);
        fflush(stderr); //stderr isn't buffered by default, but flush just in case
    }
}
static void print_pipe_w(const wchar_t* s) {
    if (outputPipe == outDefault) {
        fwprintf(stdout, L"%ls", s);
        fflush(stdout);
    } else if (outputPipe == outStdout) {
        fwprintf(stdout, L"%ls", s);
        fflush(stdout);
    } else if (outputPipe == outStderr) {
        fwprintf(stderr, L"%ls", s);
        fflush(stderr); //stderr isn't buffered by default, but flush just in case
    }
}

static void store_err(const char* s) {
    strcpy(errs[errCount % MAX_STRING_COUNT], s);
    errCount++;
}

static void print_err(const char* s, int err_line, int err_val) {    
    #ifndef HELP_HANDLER_IGNORE_ALL
    if (printErr == true) {
        if (err_val == silent) {
            return; }
        if (err_val == warning) {
            #ifndef HELP_HANDLER_IGNORE_WARN
            print_pipe(helpHandlerFuncName);
            print_pipe(":");
            print_pipe_i(err_line);
            print_pipe(" warning: ");
            print_pipe(s);
            print_pipe(". Call help_handler_disable_err() or define HELP_HANDLER_IGNORE_WARN to ignore this warning");
            #endif
        } else if (err_val == error) {
            #ifndef HELP_HANDLER_IGNORE_ERR
            print_pipe(helpHandlerFuncName);
            print_pipe(":");
            print_pipe_i(err_line);
            print_pipe(" error: ");
            print_pipe(s);
            print_pipe(". Call help_handler_disable_err() or define HELP_HANDLER_IGNORE_ERR to ignore this error");
            #endif
        }

        print_pipe("\n");
    }
    #endif

    store_err(s);
}

static int string_check(const char* s, int s_line, int err_val, const char* var_name) {
    char err_msg[MAX_STRING_LEN];

    if (var_name != NULL) {
        strcpy(err_msg, var_name); }
    if (s == NULL) {
        strcat(err_msg, " is NULL");
        print_err(err_msg, s_line,  err_val);
        return EXIT_FAILURE; }
    if (s[0] == '\0') {
        strcat(err_msg, "is empty");
        print_err(err_msg, s_line,  err_val);
        return EXIT_FAILURE; } 

    return EXIT_SUCCESS;
}
static int string_check_w(const wchar_t* s, int s_line, int err_val, const char* var_name) {
    char err_msg[MAX_STRING_LEN];

    if (var_name != NULL) {
        strcpy(err_msg, var_name); }
    if (s == NULL) {
        strcat(err_msg, " is NULL");
        print_err(err_msg, s_line,  err_val);
        return EXIT_FAILURE; }
    if (s[0] == L'\0') {
            strcat(err_msg, "is empty");
            print_err(err_msg, s_line,  err_val);
            return EXIT_FAILURE; }
    else if (sizeof(wchar_t) == 4) {

    }

    return EXIT_SUCCESS;
}

/*
 * Utilities
 */
#ifdef HELP_HANDLER_POSIX_C
static int regex_match(const char *s, const char *pattern) {
    regex_t reg;
    if (regcomp(&reg, pattern, REG_EXTENDED|REG_ICASE|REG_NOSUB) != 0) {
        print_err("failed to compile regex", __LINE__, error);
        return EXIT_FAILURE; }

    int status = regexec(&reg, s, 0, NULL, 0);
    regfree(&reg);

    if (status != 0) {
        return EXIT_FAILURE; }

    return EXIT_SUCCESS;
}
#endif

static size_t trim(char *out, size_t len, const char *str) {
    const char* end;
    size_t out_size;

    
    //Trim leading whitespace
    while(isspace((unsigned char)*str)) { str++; };
    //Trim trailing space
    end = str + strlen(str) - 1;
    while(isspace((unsigned char)*end)) { end--; };
    end++;

    //Set output size to minimum of trimmed string length and buffer size minus 1
    out_size = (end-str) < (unsigned int)len-1 ? (unsigned int)(end-str) : len-1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;


    return out_size;
}

/*
 * These static functions are only called from the two help_handler functions, but split into functions for the sake of code reuse
 */
int return_result(int result_help, int result_ver) {
    if (result_help > 0 && result_ver > 0) { return dialogHelpVer; }
    else if (result_help > 0) { return dialogHelp;
    } else if (result_ver > 0) { return dialogVer;
    } else if (result_help == 0 && result_ver == 0) { return 0;
    } else { return 0; }
}

static bool print_ver(void) {
    if (most_recent_t.ver == versionStr) {
        print_pipe(info_t.ver_str);
        return true;
    } else if (most_recent_t.ver == versionInt) {
        char n[MAX_STRING_LEN];
        sprintf(n, "%d", info_t.ver_int);
        print_pipe(n);
        return true;
    } else if (most_recent_t.ver == versionDouble) {
        char n[MAX_STRING_LEN];
        sprintf(n, "%lf", info_t.ver_double);
        print_pipe(n);
        return true; }

    return false;
}

static bool print_name(void) {
    if (strlen(info_t.name) > 0 && most_recent_t.name == nameChar) {
        print_pipe(info_t.name);
        return true;
    } else if (wcslen(info_t.name_w) > 0 && most_recent_t.name == nameWChar) {
        print_pipe_w(info_t.name_w);
        return true; }

    return false;
}


static int arg_match(int argc, char** argv, const char* regex_string, const char* fallback_string) {
    (void)regex_string; //Doesn't effect the variables. Only gets rid of unused parameter warning due to #ifdef below
    (void)fallback_string;

    if (argc > INT_MAX) {
        print_err("argument count (argc) is larger than the limit of int type", __LINE__, error);
        return helpHandlerFailure;
    } else if (argc < 1) {
        if (argc < INT_MIN) {
            print_err("argument count (argc) is smaller than the limit of int type", __LINE__, error);
        } else {
            print_err("argument count (argc) is 0 or less (should always be at least 1)..", __LINE__, error); }

        return helpHandlerFailure; }

    if (argv == NULL) {
        print_err("argument value (argv) is NULL", __LINE__, error);
        return helpHandlerFailure; }
    if (string_check(*argv, __LINE__, error, "argument value (argv)") == EXIT_FAILURE) {
        return helpHandlerFailure; }

    for (int i = 1; i < argc; i++) { //Start from 1 to skip executable name
        if (argv[i] == NULL) {
            print_err("argument count (argc) exceeds actual number of arguments", __LINE__, error);
            return helpHandlerFailure; }
    
        #ifdef HELP_HANDLER_POSIX_C
        if (regex_string != NULL) {
            if (regex_match(argv[i], regex_string) == EXIT_SUCCESS) {
                return i; }
        }
        #else
        if (fallback_string != NULL) {
            #if defined _WIN32 || defined _WIN64
            int result = _stricmp(argv[i], fallback_string);
            #else
            int result = strcasecmp(argv[i], fallback_string);
            #endif

            if (result == 0) {
                return i; }
        }
        #endif
    }

    return 0;
}

static int help_handler_sub(int argc, char** argv) {
    #ifndef HELP_HANDLER_POSIX_C
    //Casts to silence C++ warnings
    char* help_lex[] = {
        (char*)"h", (char*)"-h", (char*)"--h",
        (char*)"help", (char*)"-help", (char*)"--help",
        (char*)"hhelp", (char*)"heelp", (char*)"hellp", (char*)"helpp",
        (char*)"-hhelp", (char*)"-heelp", (char*)"-hellp", (char*)"-helpp",
        (char*)"--hhelp", (char*)"--heelp", (char*)"--hellp", (char*)"--helpp" };
    char* ver_lex[] = { 
        (char*)"v", (char*)"-v", (char*)"--v",
        (char*)"version", (char*)"-version", (char*)"--version",
        (char*)"vversion", (char*)"veersion", (char*)"verrsion", (char*)"verssion", (char*)"versiion", (char*)"versioon", (char*)"versionn",
        (char*)"-vversion", (char*)"-veersion", (char*)"-verrsion", (char*)"-verssion", (char*)"-versiion", (char*)"-versioon", (char*)"-versionn",
        (char*)"--vversion", (char*)"--veersion", (char*)"--verrsion", (char*)"--verssion", (char*)"--versiion", (char*)"--versioon", (char*)"--versionn" };
    int i = 0;

    if (options_t.extra_strings != true) {
        ver_lex[0]  = "";ver_lex[1]  = "";ver_lex[2]  = "";
        help_lex[0] = "";help_lex[1] = "";help_lex[2] = ""; 
        i = 3; } //First three elements of help/ver array should be abbreviated versions, hence setting the array index to the fourth element

    int result_help, result_ver = 0;
    for (; *help_lex[i] != '\0'; i++) {
        int result;
        result = arg_match(argc, argv, NULL, help_lex[i]);
        if (help_handler_is_err(result)) { return result; }

        if (result > 0) { result_help = result; }
        for (; *ver_lex[i] != '\0'; j++) {
            result = arg_match(argc, argv, NULL, ver_lex[i]);
            if (help_handler_is_err(result)) { return result; }
            if (result > 0) { result_ver = result; }
        }
    }

    int r = return_result(result_help, result_ver);
    if (r != 0) { return r; }
    #else
    char help_lex[MAX_STRING_LEN];
    char ver_lex[MAX_STRING_LEN];
    if (options_t.extra_strings == true) {
        strcpy(help_lex, "-{0,}h{1,}e{1,}l{1,}p{1,}(.*)|-{0,}h{1,}$");
        strcpy(ver_lex, "-{0,}v{1,}e{1,}r{1,}s{0,}i{0,}o{0,}n{0,}(.*)|^-{0,}v$");
    } else {
        strcpy(help_lex, "-{0,}h{1,}e{1,}l{1,}p{1,}(.*)"); 
        strcpy(ver_lex, "-{0,}v{1,}e{1,}r{1,}s{0,}i{0,}o{0,}n{0,}(.*)"); }

    int result_help = arg_match(argc, argv, help_lex, NULL);
    if (help_handler_is_err(result_help)) { return result_help; }
    int result_ver  = arg_match(argc, argv, ver_lex, NULL);
    if (help_handler_is_err(result_ver)) { return result_ver; }
    int r = return_result(result_help, result_ver);
    if (r != 0) { return r; }
    #endif

    if (true == options_t.unknown_arg_help && argc > 1) {
        if (argc > 2) {
            print_pipe("Unknown arguments given\n"); 
        } else {
            print_pipe("Unknown argument given\n"); }

        return helpHandlerSuccess;
    }

    return helpHandlerSuccess;
}




/**************************/
/*                        */
/**** PUBLIC FUNCTIONS ****/
/*                        */
/**************************/
bool help_handler_is_err(int errorCode) {
    if (errorCode < 0) { return true; }
    else { return false; };
}

void help_handler_disable_err(bool disableErrorOutput) {
    printErr = disableErrorOutput;
}

void help_handler_print_err(void) {
    for (size_t i = 0; i < errCount; i++) {
        print_pipe(helpHandlerFuncName);
        print_pipe(" ERROR ");
        print_pipe(errs[i]);
    }
}

char* help_handler_get_err(void) {
    if (errs[errCount][0] != '0') {
        errCount--;
        return errs[errCount]; }

    return NULL;
}

#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED
void help_handler_pipe_s(const char* output_pipe) {
#else
void help_handler_pipe(const char* output_pipe) {
#endif
    if (string_check(output_pipe, __LINE__, warning, "output_pipe") == EXIT_FAILURE) {
        return; }

    #if defined _WIN32 || defined _WIN64
    if (_stricmp(output_pipe, "stdout") == 0) {
        outputPipe = outStdout;
    } else if (_stricmp(output_pipe, "stderr") == 0) {
        outputPipe = outStderr;
    } else {
        outputPipe = outDefault; }
    #else
    if (strcasecmp(output_pipe, "stdout") == 0) {
        outputPipe = outStdout;
    } else if (strcasecmp(output_pipe, "stderr") == 0) {
        outputPipe = outStderr;
    } else {
        outputPipe = outDefault; }
    #endif
}

void help_handler_pipe_i(int output_pipe) {
    if (output_pipe == outStdout) {
        outputPipe = output_pipe;
    } else if (output_pipe == outStderr) {
        outputPipe = output_pipe;
    } else {
        outputPipe = outDefault;
    }
}

void help_handler_config(bool extra_strings, bool no_arg_help, bool unknown_arg_help) {
    options_t.extra_strings    = extra_strings;
    options_t.no_arg_help      = no_arg_help;
    options_t.unknown_arg_help = unknown_arg_help;
}

#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED
int help_handler_version_s(const char* ver) { //Parent function
#else
int help_handler_version(const char* ver) {
#endif
    if (string_check(ver, __LINE__, error, "ver") == EXIT_FAILURE) { return helpHandlerFailure; }
    if (strlen(ver)+1 >= sizeof(info_t.ver_str)) {
        print_err("given version string is larger than allowed", __LINE__, error);
        return helpHandlerFailure; }

    char* version = (char*)malloc(strlen(ver)+1);
    trim(version, strlen(ver)+1, ver);
    strcpy(info_t.ver_str, version);
    most_recent_t.ver = versionStr;

    free(version);
    return helpHandlerSuccess;
} 
void help_handler_version_i(unsigned int ver) {
    info_t.ver_int = ver;
    most_recent_t.ver = versionInt;
}
void help_handler_version_d(double ver) {
    info_t.ver_double = ver;
    most_recent_t.ver = versionDouble;
}


#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED
int help_handler_name_s(const char* app_name) { //Parent function
#else
int help_handler_name(const char* app_name) {
#endif
    if (string_check(app_name, __LINE__, error, "app_name") == EXIT_FAILURE) { return helpHandlerFailure; }
    if (strlen(app_name)+1 >= sizeof(info_t.name)) {
        print_err("given app name is larger than allowed", __LINE__, error);
        return helpHandlerFailure; }

    char* name = (char*)malloc(strlen(app_name)+1);
    trim(name, strlen(app_name)+1, app_name);
    strcpy(info_t.name, name);
    most_recent_t.name = nameChar;

    return helpHandlerSuccess;
}

int help_handler_name_w(const wchar_t* app_name) { //Parent function
    if (string_check((char*)app_name, __LINE__, error, "app_name") == EXIT_FAILURE) { return helpHandlerFailure; }
    if (wcslen(app_name)+1 >= sizeof(info_t.name)) {
        print_err("given app name (wchar type) is larger than allowed", __LINE__, warning);
        return helpHandlerFailure; }

    wcscpy(info_t.name_w, app_name);
    most_recent_t.name = nameWChar;

    return helpHandlerSuccess;
}

#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED
int help_handler_info_s(const char* app_name, const char* ver) {
#else
int help_handler_info(const char* app_name, const char* ver) {
#endif
    int result = help_handler_version(ver);
    if (help_handler_is_err(result)) { return result; }
    return help_handler_name(app_name);
}
int help_handler_info_i(const char* app_name, unsigned int ver) {
    help_handler_version_d(ver);
    return help_handler_name(app_name);
}
int help_handler_info_d(const char* app_name, double ver) {
    help_handler_version_d(ver);
    return help_handler_name(app_name);
}

int help_handler_info_w(wchar_t* app_name, const char* ver) {
    int result = help_handler_version(ver);
    if (help_handler_is_err(result)) { return result; }
    return help_handler_name_w(app_name);
}
int help_handler_info_wi(wchar_t* app_name, unsigned int ver) {
    help_handler_version_i(ver);
    return help_handler_name_w(app_name);
}
int help_handler_info_wd(wchar_t* app_name, double ver) {
    help_handler_version_d(ver);
    return help_handler_name_w(app_name);
}


#ifdef HELP_HANDLER_OVERLOAD_SUPPORTED
int help_handler_s(int argc, char** argv, const char* help_dialogue) {
#else
int help_handler(int argc, char** argv, const char* help_dialogue) {
#endif
    char* help = (char*)malloc(strlen("No usage help is available")+1); //Cast to silence C++ warning    
    if (string_check(help_dialogue, __LINE__, silent, NULL) == EXIT_SUCCESS) {
        help = (char*)realloc(help, strlen(help_dialogue)+1);
        strcpy(help, help_dialogue); } 
    if (argc == 1 && options_t.no_arg_help == true) {
        if (print_name()) { 
            print_pipe(" "); }
        print_pipe(help); 
        print_pipe("\n");
 
        free(help);
        return helpHandlerSuccess;
    }

    int result = help_handler_sub(argc, argv);
    if (result == dialogHelpVer) {
        if (print_name()) {
            print_pipe(" Version "); }
        print_ver();
        print_pipe("\n");
        print_pipe(help);
    } else if (result == dialogHelp) {
        if (print_name()) {
            print_pipe(" "); }
        print_pipe(help);
    } else if (result == dialogVer) {
        print_ver();
    } else if (help_handler_is_err(result)) { return result; }
    
    print_pipe("\n");


    free(help);
    return helpHandlerSuccess;
}

int help_handler_w(int argc, char** argv, const wchar_t* help_dialogue) {
    wchar_t* help = (wchar_t*)L"No usage help is available"; //Cast to silence C++ warning
    if (string_check_w(help_dialogue, __LINE__, silent, NULL) == EXIT_FAILURE) {
        help = (wchar_t*)malloc(wcslen(help_dialogue)+1);
        wcscpy(help, help_dialogue); }
    if (argc == 1 && options_t.no_arg_help == true) {
        if (print_name()) { 
            print_pipe(" "); }
        print_pipe_w(help); 
        print_pipe("\n");
        return helpHandlerSuccess;
    }

    int result = help_handler_sub(argc, argv);
    if (result == dialogHelpVer) {
        if (print_name()) {
            print_pipe(" Version "); }
        print_ver();
        print_pipe("\n");
        print_pipe_w(help);
    } else if (result == dialogHelp) {
        if (print_name()) {
            print_pipe(" "); }
        print_pipe_w(help);
    } else if (result == dialogVer) {
        print_ver();
    } else if (help_handler_is_err(result)) { return result; }
    
    print_pipe("\n");


    free(help);
    return helpHandlerSuccess;
}

int help_handler_f(int argc, char** argv, const char* file_name) {
    if (string_check(file_name, __LINE__, error, "file_name") == EXIT_FAILURE) {
        return helpHandlerFailure; }

    FILE* fp = fopen(file_name, "rb"); //Windows mangles newlines in r, so use rb
    if (fp == NULL) {
        print_err(strerror(errno), __LINE__, error);
        return helpHandlerFailure; }

    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char* contents = NULL;
    if ((contents = (char*)malloc((unsigned long)size+1)) == NULL) { 
        print_err("failed to allocate memory", __LINE__, error);
        return helpHandlerFailure; }

    size_t n_items = fread(contents, 1, (size_t)size, fp);
    if (n_items == 0) {
        print_err("given help file is empty", __LINE__, error); 
        return helpHandlerFailure; }
    if ((long)n_items < size) {
        print_err(strerror(errno), __LINE__, error);
        return helpHandlerFailure; }
    if (fclose(fp) == EOF) {
        print_err(strerror(errno), __LINE__, error);
        return helpHandlerFailure; }

    int return_val = help_handler(argc, argv, (const char*)contents);
    if (contents) { free(contents); }
    return return_val;
}


#undef MAX_STRING_LEN
#undef MAX_STRING_COUNT
#undef HELP_HANDLER_POSIX_C
#undef HELP_HANDLER_OVERLOAD_SUPPORTED
#endif  /* HELP_HANDLER_H */
