#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,     // | the output of the left command is the input for the right command
    TOKEN_AMP,      // & execute it in the background
    TOKEN_SEMI,     // ; execute the next command after the previous one
    TOKEN_LT,       // < takes the command standard input from this file
    TOKEN_GT,       // > send or replaces the output into the output.txt file
    TOKEN_GTGT,     // >> Appends the output into output.txt file
    TOKEN_EOF,      // End of input line
    TOKEN_ERROR     // Lexical error (e.g., unclosed quote)
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    struct Token *next;
} Token;
//Takes the command the user gives and breaks it into tokens and for error returns -1
int lex_line(const char *input, Token **out_tokens);
//Checks the tokens with grammer and validates if 1 then valid else 0
int parse_line(Token *head);
// Frees memory allocated for token list
void free_tokens(Token *head);

#endif