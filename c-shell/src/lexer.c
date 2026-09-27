#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//this struct is for creating a token structure
static Token *create_token(TokenType type, const char *value) {
    Token *t = malloc(sizeof(Token));
    if (!t) return NULL;
    t->type = type;
    t->value = value ? strdup(value) : NULL;
    t->next = NULL;
    return t;
}
//This creates a linked list of tokens and iterates through it and frees the memory later
void free_tokens(Token *head) {
    while (head) {
        Token *tmp = head;
        head = head->next;
        if (tmp->value) free(tmp->value);
        free(tmp);
    }
}
/*Here the command is taken and is converted into linked list of tokens
whitespaces are ignored
operatoes are created using special tokens
words are the collected characters into word
end of input create token_eof
input is the input and it should not be changed by mistake so const is used
out_tokens are where the tokens are stored in a linked list*/
int lex_line(const char *input, Token **out_tokens) {
    Token head = {0};
    Token *curr = &head;
    //curr is the temp head for keeping track of the linked list actual head
    int i = 0;
    int len = strlen(input);

    while (i < len) {
        //to skip whitespace
        if (isspace((unsigned char)input[i])) {
            i++;
            continue;
        }

        //check for >> first before >
        if (input[i] == '>' && input[i + 1] == '>') {
            curr->next = create_token(TOKEN_GTGT, ">>");
            curr = curr->next;
            i += 2;
            continue;
        }

        // Single-character operators
        if (input[i] == '|') {
            curr->next = create_token(TOKEN_PIPE, "|");
            curr = curr->next; i++; continue;
        }
        if (input[i] == '&') {
            curr->next = create_token(TOKEN_AMP, "&");
            curr = curr->next; i++; continue;
        }
        if (input[i] == ';') {
            curr->next = create_token(TOKEN_SEMI, ";");
            curr = curr->next; i++; continue;
        }
        if (input[i] == '<') {
            curr->next = create_token(TOKEN_LT, "<");
            curr = curr->next; i++; continue;
        }
        if (input[i] == '>') {
            curr->next = create_token(TOKEN_GT, ">");
            curr = curr->next; i++; continue;
        }

        //if it is not a character then it must be a word
        char word_buf[4096];
        int w = 0;
        int error = 0;

        while (i < len && !isspace((unsigned char)input[i]) &&
               input[i] != '|' && input[i] != '&' && input[i] != ';' &&
               input[i] != '<' && input[i] != '>') {
            //the backslash says treat the next character literally or specially like \n
            if (input[i] == '\\') {
                if (i + 1 >= len) { 
                    error = 1;
                    break; //here after backslash if nothing is present then it is invalid and the process breaks
                }
                word_buf[w++] = input[i + 1];
                i += 2; //this takes the space as the part of the word by placing backslash
            } else if (input[i] == '\'') { // Single quote when present is taken as a single word even if spaces are present
                i++;
                while (i < len && input[i] != '\'') {
                    word_buf[w++] = input[i++];//keep copying until we find the other '
                }
                if (i >= len) { error = 1; break; } //if closing ' is absent then it is error
                i++; // Skip closing quote
            } else if (input[i] == '"') { // Double quote also have the same privilages but has more flexibility
                i++;
                while (i < len && input[i] != '"') {
                    if (input[i] == '\\') {
                        if (i + 1 < len && (input[i + 1] == '"' || input[i + 1] == '\\')) {
                            word_buf[w++] = input[i + 1];
                            i += 2;
                        } else {
                            word_buf[w++] = input[i++];
                        }
                    } else {
                        word_buf[w++] = input[i++];
                    }
                }
                if (i >= len) { error = 1; break; } // Unclosed quote
                i++; // Skip closing quote
            } else {//else other that \ ' and " all the other characters are treated as normal
                word_buf[w++] = input[i++];
            }
        }

        if (error) {
            free_tokens(head.next); //if already the tokens are created since it is an erro it is needed to be cleaed
            return -1;//lexing failed
        }

        word_buf[w] = '\0';//this is to finish the word
        curr->next = create_token(TOKEN_WORD, word_buf);
        curr = curr->next;
    }

    curr->next = create_token(TOKEN_EOF, NULL);
    *out_tokens = head.next; //gives the list back
    return 0;
}


//checks if the pieces of the command are arranged in the correct order
int parse_line(Token *head) {
    if (!head || head->type == TOKEN_EOF) return 1; // Empty line is valid

    Token *curr = head;

    // command must begin with WORD
    if (curr->type != TOKEN_WORD) return 0;
    //keep checking tokens till the eof or no tokens left
    while (curr && curr->type != TOKEN_EOF) {
        if (curr->type == TOKEN_WORD) {
            curr = curr->next;
        } else if (curr->type == TOKEN_LT || curr->type == TOKEN_GT || curr->type == TOKEN_GTGT) {
            // Redirection operators must be followed by a file target that is a word
            curr = curr->next;
            if (!curr || curr->type != TOKEN_WORD) return 0;
            curr = curr->next;
        } else if (curr->type == TOKEN_PIPE || curr->type == TOKEN_SEMI) {
            // Pipe / Semicolon should be followed by a new Command 
            curr = curr->next;
            if (!curr || curr->type != TOKEN_WORD) return 0;
            curr = curr->next;
        } else if (curr->type == TOKEN_AMP) {
            // Background operator & can either end the line or be followed by another CMD
            curr = curr->next;
            if (curr->type == TOKEN_EOF) return 1;
            if (curr->type != TOKEN_WORD) return 0;
            curr = curr->next;
        } else {
            return 0;
        }
    }

    return 1;
}
