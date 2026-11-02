#include "scanner.h"

typedef enum
{
    STATE_START,
    STATE_IDENTIFIER,
    STATE_INT_LITERAL,
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT,
    STATE_LESSER,
    STATE_BIGGER,
    STATE_ASSIGNMENT,
    STATE_EXCLAMATION,
    STATE_ZERO,
    STATE_INT_UNDERSCORE,
    STATE_DOT,
    STATE_DOUBLE_LITERAL,
    STATE_DOUBLE_UNDERSCORE,
    STATE_EXP,
    STATE_EXP_SIGN,
    STATE_EXP_LITERAL,
    STATE_EXP_UNDERSCORE,
    STATE_STRING,
    STATE_STRING_ESC,
    STATE_MULTILINE_STRING
} State;


typedef struct
{
    char* data;
    int len;
    int capacity;
} DynamicString;

void* safeMalloc(int size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
    {
        fprintf(stderr, "Out of memory!\n");
        exit(99);
    }
    return ptr;
}

bool strInit(DynamicString* s)
{
    s->capacity = 16;
    s->len = 0;
    s->data = (char*)safeMalloc(s->capacity * sizeof(char));
    s->data[0] = '\0';
    return true;
}

void strFree(DynamicString* s)
{
    if (s->data != NULL)
    {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
    s->capacity = 0;
}

void strAppend(DynamicString* s, char c)
{
    if (s->len + 1 >= s->capacity)
    {
        s->capacity *= 2;

        char* newData = (char*)realloc(s->data, s->capacity * sizeof(char));
        if (newData == NULL)
        {
            fprintf(stderr, "Out of memory in realloc!\n");
            exit(99);
        }
        s->data = newData;
    }
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
}

void strClear(DynamicString* s)
{
    s->len = 0;
    if (s->data != NULL)
    {
        s->data[0] = '\0';
    }
}

typedef struct
{
    char* keyword;
    Type type;
} Keyword;

Keyword Keywords[] = {
    {"def", TYPE_DEF},   {"do", TYPE_DO},         {"Double", TYPE_DOUBLE},
    {"else", TYPE_ELSE}, {"if", TYPE_IF},         {"import", TYPE_IMPORT},
    {"Int", TYPE_INT},   {"return", TYPE_RETURN}, {"String", TYPE_STRING},
    {"then", TYPE_THEN}, {"Unit", TYPE_UNIT},     {"val", TYPE_VAL},
    {"var", TYPE_VAR},   {"while", TYPE_WHILE},   {NULL, 0}};

void scannerInit(Scanner* scanner, FILE* stream)
{
    scanner->line = 1;
    scanner->firstRead = 1;
    scanner->stream = stream;

    scanner->indentStack[0] = 0;
    scanner->indentTop = 0;
    scanner->pendingDedents = 0;
    scanner->atLineStart = true;
}

int getToken(Token* token, Scanner* scanner)
{
    State state = STATE_START;
    token->line = scanner->line;

    if (scanner->pendingDedents > 0)
    {
        scanner->pendingDedents--;
        token->type = TYPE_DEDENT;
        return 0;
    }

    while (scanner->atLineStart)
    {
        int spaces = 0;
        int c;

        while ((c = fgetc(scanner->stream)) == ' ')
        {
            spaces++;
        }

        if (c == '\t')
        {
            token->type = TYPE_ERROR;
            return 1;
        }

        if (c == '/')
        {
            int next = fgetc(scanner->stream);
            if (next == '/')
            {
                while ((c = fgetc(scanner->stream)) != '\n' && c != EOF)
                {
                }
                if (c == '\n') scanner->line++;
                continue;
            }
            ungetc(next, scanner->stream);
        }


        if (c == EOF)
        {
            if (scanner->indentTop > 0)
            {
                scanner->indentTop--;
                token->type = TYPE_DEDENT;
                return 0;
            }
            token->type = TYPE_EOF;
            return 0;
        }

        if (c == '\n')
        {
            scanner->line++;
            continue;
        }

        ungetc(c, scanner->stream);
        scanner->atLineStart = false;

        if (spaces > scanner->indentStack[scanner->indentTop])
        {
            scanner->indentTop++;
            scanner->indentStack[scanner->indentTop] = spaces;
            token->type = TYPE_INDENT;
            return 0;
        }

        if (spaces < scanner->indentStack[scanner->indentTop])
        {
            int targetIndex = -1;
            for (int i = scanner->indentTop - 1; i >= 0; i--)
            {
                if (scanner->indentStack[i] == spaces)
                {
                    targetIndex = i;
                    break;
                }
            }
            if (targetIndex == -1)
            {
                token->type = TYPE_ERROR;
                return 1;
            }

            int dedentCount = scanner->indentTop - targetIndex;
            scanner->indentTop = targetIndex;

            scanner->pendingDedents = dedentCount - 1;
            token->type = TYPE_DEDENT;
            return 0;
        }

        break;
    }

    int commentLevel = 0;
    DynamicString attr;

    while (true)
    {
        int c = fgetc(scanner->stream);
        switch (state)
        {
            case STATE_START:
                if (c == ' ' || c == '\t')
                {
                    continue;
                }


                if (c == '\n')
                {
                    scanner->line++;
                    token->line = scanner->line;
                    scanner->atLineStart = true;
                    token->type = TYPE_EOL;
                    return 0;
                }

                if (c == ')')
                {
                    token->type = TYPE_RIGHT_PAREN;
                    return 0;
                }

                if (c == ':')
                {
                    token->type = TYPE_DOUBLE_DOT;
                    return 0;
                }

                if (c == ',')
                {
                    token->type = TYPE_COMMA;
                    return 0;
                }

                if (c == ';')
                {
                    token->type = TYPE_SEMICOLON;
                    return 0;
                }

                if (c == '-')
                {
                    token->type = TYPE_SUB;
                    return 0;
                }

                if (c == '+')
                {
                    token->type = TYPE_ADD;
                    return 0;
                }

                if (c == '*')
                {
                    token->type = TYPE_MUL;
                    return 0;
                }

                if (c == '/')
                {
                    state = STATE_SLASH;
                    break;
                }

                if (c == '=')
                {
                    state = STATE_ASSIGNMENT;
                    break;
                }

                if (c == '<')
                {
                    state = STATE_LESSER;
                    break;
                }

                if (c == '>')
                {
                    state = STATE_BIGGER;
                    break;
                }

                if (c == '!')
                {
                    state = STATE_EXCLAMATION;
                    break;
                }

                if (c == '0')
                {
                    strInit(&attr);
                    strClear(&attr);
                    strAppend(&attr, c);
                    state = STATE_ZERO;
                    break;
                }

                if (c == '"')
                {
                    int c2 = fgetc(scanner->stream);
                    if (c2 == '"')
                    {
                        int c3 = fgetc(scanner->stream);
                        if (c3 == '"')
                        {
                            strInit(&attr);
                            strClear(&attr);
                            state = STATE_MULTILINE_STRING;
                            break;
                        }
                        else
                        {
                            ungetc(c3, scanner->stream);
                            token->type = TYPE_STRING_LITERAL;
                            token->value.string = (char*)safeMalloc(1);
                            token->value.string[0] = '\0';
                            return 0;
                        }
                    }
                    ungetc(c2, scanner->stream);
                    strInit(&attr);
                    strClear(&attr);
                    state = STATE_STRING;
                    break;
                }

                if (isdigit(c))
                {
                    strInit(&attr);
                    strClear(&attr);
                    strAppend(&attr, c);
                    state = STATE_INT_LITERAL;
                    break;
                }

                if (isalpha(c) || c == '_')
                {
                    strInit(&attr);
                    strClear(&attr);
                    strAppend(&attr, c);
                    state = STATE_IDENTIFIER;
                    break;
                }

                if (c == '@')
                {
                    char expected[] = "main";
                    for (int i = 0; i < 4; i++)
                    {
                        int next = fgetc(scanner->stream);
                        if (next != expected[i])
                        {
                            token->type = TYPE_ERROR;
                            return 1;
                        }
                    }

                    int next = fgetc(scanner->stream);

                    if (isalnum(next) || next == '_')
                    {
                        token->type = TYPE_ERROR;
                        return 1;
                    }

                    ungetc(next, scanner->stream);

                    token->type = TYPE_MAIN;
                    return 0;
                }

                if (c == '(')
                {
                    int next = fgetc(scanner->stream);
                    if (next == ')')
                    {
                        token->type = TYPE_UNIT_LITERAL;
                        return 0;
                    }
                    ungetc(next, scanner->stream);
                    token->type = TYPE_LEFT_PAREN;
                    return 0;
                }

                if (c == '.')
                {
                    token->type = TYPE_DOT;
                    return 0;
                }


                if (c == EOF)
                {
                    if (scanner->indentTop > 0)
                    {
                        scanner->indentTop--;
                        token->type = TYPE_DEDENT;
                        return 0;
                    }
                    scanner->atLineStart = false;
                    token->type = TYPE_EOF;
                    return 0;
                }
                token->type = TYPE_ERROR;
                return 1;
                break;


            case STATE_ASSIGNMENT:
                if (c == '=')
                {
                    token->type = TYPE_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_ASSIGNMENT;
                return 0;

            case STATE_LESSER:
                if (c == '=')
                {
                    token->type = TYPE_LESSER_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_LESSER;
                return 0;

            case STATE_BIGGER:
                if (c == '=')
                {
                    token->type = TYPE_BIGGER_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_BIGGER;
                return 0;

            case STATE_EXCLAMATION:
                if (c == '=')
                {
                    token->type = TYPE_NOT_EQUAL;
                    return 0;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_SLASH:
                if (c == '/')
                {
                    state = STATE_LINE_COMMENT;
                    break;
                }
                if (c == '*')
                {
                    commentLevel = 1;
                    state = STATE_BLOCK_COMMENT;
                    break;
                }
                ungetc(c, scanner->stream);
                token->type = TYPE_DIV;
                return 0;

            case STATE_BLOCK_COMMENT:
                if (c == EOF)
                {
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '\n')
                {
                    scanner->line++;
                    break;
                }

                if (c == '/')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '*')
                    {
                        commentLevel++;
                    }
                    else
                    {
                        ungetc(next, scanner->stream);
                    }
                    break;
                }

                if (c == '*')
                {
                    int next = fgetc(scanner->stream);
                    if (next == '/')
                    {
                        commentLevel--;
                        if (commentLevel == 0)
                        {
                            state = STATE_START;
                        }
                    }
                    else
                    {
                        ungetc(next, scanner->stream);
                    }
                }

                break;

            case STATE_LINE_COMMENT:
                if (c == '\n')
                {
                    scanner->line++;
                    token->line = scanner->line;
                    token->type = TYPE_EOL;
                    return 0;
                }

                if (c == EOF)
                {
                    token->type = TYPE_EOF;
                    return 0;
                }
                break;

            case STATE_ZERO:
                if (c == '.')
                {
                    strAppend(&attr, c);
                    state = STATE_DOT;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    strAppend(&attr, c);
                    state = STATE_EXP;
                    break;
                }

                if (isdigit(c))
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_INT_LITERAL;
                token->value.numberInt = 0;
                strFree(&attr);
                return 0;

            case STATE_INT_LITERAL:

                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    break;
                }

                if (c == '_')
                {
                    state = STATE_INT_UNDERSCORE;
                    break;
                }

                if (c == '.')
                {
                    strAppend(&attr, c);
                    state = STATE_DOT;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    strAppend(&attr, c);
                    state = STATE_EXP;
                    break;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_INT_LITERAL;
                token->value.numberInt = strtoll(attr.data, NULL, 10);
                strFree(&attr);
                return 0;

            case STATE_INT_UNDERSCORE:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_INT_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;
            case STATE_DOT:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_DOUBLE_LITERAL;
                    break;
                }
                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_DOUBLE_LITERAL:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    break;
                }

                if (c == '_')
                {
                    state = STATE_DOUBLE_UNDERSCORE;
                    break;
                }

                if (c == 'e' || c == 'E')
                {
                    strAppend(&attr, c);
                    state = STATE_EXP;
                    break;
                }

                if (c == 'd' || c == 'D')
                {
                    token->type = TYPE_DOUBLE_LITERAL;
                    token->value.numberFloat = strtod(attr.data, NULL);
                    strFree(&attr);
                    return 0;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_DOUBLE_LITERAL;
                token->value.numberFloat = strtod(attr.data, NULL);
                strFree(&attr);
                return 0;

            case STATE_DOUBLE_UNDERSCORE:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_DOUBLE_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_EXP:
                if (c == '+' || c == '-')
                {
                    strAppend(&attr, c);
                    state = STATE_EXP_SIGN;
                    break;
                }

                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_EXP_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_EXP_SIGN:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_EXP_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;


            case STATE_EXP_LITERAL:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    break;
                }

                if (c == '_')
                {
                    state = STATE_EXP_UNDERSCORE;
                    break;
                }

                if (c == 'd' || c == 'D')
                {
                    token->type = TYPE_DOUBLE_LITERAL;
                    token->value.numberFloat = strtod(attr.data, NULL);
                    strFree(&attr);
                    return 0;
                }

                ungetc(c, scanner->stream);
                token->type = TYPE_DOUBLE_LITERAL;
                token->value.numberFloat = strtod(attr.data, NULL);
                strFree(&attr);
                return 0;

            case STATE_EXP_UNDERSCORE:
                if (isdigit(c))
                {
                    strAppend(&attr, c);
                    state = STATE_EXP_LITERAL;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_IDENTIFIER:
                if (isalnum(c) || c == '_')
                {
                    strAppend(&attr, c);
                    break;
                }
                ungetc(c, scanner->stream);

                if (attr.len == 1 && attr.data[0] == '_')
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                token->type = TYPE_IDENTIFIER;

                for (int i = 0; Keywords[i].keyword != NULL; i++)
                {
                    if (strcmp(attr.data, Keywords[i].keyword) == 0)
                    {
                        token->type = Keywords[i].type;
                        break;
                    }
                }

                if (token->type == TYPE_IDENTIFIER)
                {
                    token->value.identifier = (char*)safeMalloc(attr.len + 1);
                    strcpy(token->value.identifier, attr.data);
                }

                strFree(&attr);
                return 0;

            case STATE_STRING:
                if (c == '"')
                {
                    token->type = TYPE_STRING_LITERAL;
                    token->value.string = (char*)safeMalloc(attr.len + 1);
                    strcpy(token->value.string, attr.data);

                    strFree(&attr);
                    return 0;
                }

                if (c == '\\')
                {
                    state = STATE_STRING_ESC;
                    break;
                }

                if (c == '\n' || c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                strAppend(&attr, c);
                break;

            case STATE_STRING_ESC:
                if (c == '"')
                {
                    strAppend(&attr, c);
                    state = STATE_STRING;
                    break;
                }

                if (c == 'n')
                {
                    strAppend(&attr, '\n');
                    state = STATE_STRING;
                    break;
                }

                if (c == 't')
                {
                    strAppend(&attr, '\t');
                    state = STATE_STRING;
                    break;
                }

                if (c == 'r')
                {
                    strAppend(&attr, '\r');
                    state = STATE_STRING;
                    break;
                }

                if (c == '\\')
                {
                    strAppend(&attr, c);
                    state = STATE_STRING;
                    break;
                }

                strFree(&attr);
                token->type = TYPE_ERROR;
                return 1;

            case STATE_MULTILINE_STRING:
                if (c == EOF)
                {
                    strFree(&attr);
                    token->type = TYPE_ERROR;
                    return 1;
                }

                if (c == '\n')
                {
                    scanner->line++;
                    strAppend(&attr, '\n');
                    break;
                }

                if (c == '"')
                {
                    int c2 = fgetc(scanner->stream);
                    if (c2 == '"')
                    {
                        int c3 = fgetc(scanner->stream);
                        if (c3 == '"')
                        {
                            token->type = TYPE_STRING_LITERAL;
                            token->value.string =
                                (char*)safeMalloc(attr.len + 1);
                            strcpy(token->value.string, attr.data);
                            strFree(&attr);
                            return 0;
                        }

                        strAppend(&attr, '"');
                        strAppend(&attr, '"');
                        ungetc(c3, scanner->stream);
                        break;
                    }
                    strAppend(&attr, '"');
                    ungetc(c2, scanner->stream);
                    break;
                }
                strAppend(&attr, c);
                break;
        }
    }
    return 0;
}