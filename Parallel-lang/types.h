#ifndef types_h
#define types_h

#include <stdlib.h>

struct linkedList_Node {
	struct linkedList_Node *next;
	uint8_t data[];
};
typedef struct linkedList_Node linkedList_Node;

void *linkedList_addNode(linkedList_Node **head, unsigned long size);

void linkedList_freeList(linkedList_Node **head);

typedef struct {
	int line;
	int columnStart;
	int columnEnd;
} SourceLocation;

typedef enum {
	TokenType_word,
	TokenType_string,
	TokenType_separator
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	char value[];
} Token;

typedef enum {
	ASTnodeType_type,
	ASTnodeType_function,
	ASTnodeType_number
} ASTnodeType;

typedef struct {
	char *name;
} ASTnode_type;

typedef struct {
	char *name;
	ASTnode_type returnType;
	linkedList_Node *arguments;
	linkedList_Node *codeBlock;
} ASTnode_function;

typedef struct {
	int64_t *value;
} ASTnode_number;

typedef struct {
	ASTnodeType type;
	SourceLocation location;
	char value[];
} ASTnode;

#endif /* types_h */
