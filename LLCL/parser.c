#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "report.h"

#define endIfCurrentIsEmpty()\
if (*current == NULL) {\
	addStringToReportMsg("unexpected end of file");\
	compileError(FI, token->location);\
}

#define CURRENT_IS_NOT_SEMICOLON *current == NULL || (((Token *)((*current)->data))->type != TokenType_separator || SubString_string_cmp(&((Token *)((*current)->data))->subString, ";") != 0)

int getIfNodeShouldHaveSemicolon(ASTnode *node) {
	return node->nodeType != ASTnodeType_constrainedType &&
	node->nodeType != ASTnodeType_function &&
	node->nodeType != ASTnodeType_while &&
	node->nodeType != ASTnodeType_if;
}

int64_t intPow(int64_t base, int64_t exponent) {
	if (exponent == 0) {
		return 1;
	}
	int64_t result = base;
	for (int i = 1; i < exponent; i++) {
		result *= base;
	}
	return result;
}

ASTnode_number parseInt(FileInformation *FI, linkedList_Node **current) {
	ASTnode_number node = {};
	
	Token *token = ((Token *)((*current)->data));
	endIfCurrentIsEmpty()
	
	if (token->type != TokenType_number) abort();
	
	int64_t accumulator = 0;
	
	long stringLength = token->subString.length;
	
	for (int index = 0; index < stringLength; index++) {
		char character = token->subString.start[index];
		int number = character - '0';
		
		accumulator += intPow(10, stringLength-index-1) * number;
	}
	
	node.value = accumulator;
	
	return node;
}

int getOperatorPrecedence(SubString *subString) {
	if (SubString_string_cmp(subString, "=") == 0) {
		return 1;
	}
	
	else if (SubString_string_cmp(subString, "||") == 0) {
		return 2;
	}
	
	else if (SubString_string_cmp(subString, "&&") == 0) {
		return 3;
	}
	
	else if (
		SubString_string_cmp(subString, "==") == 0 ||
		SubString_string_cmp(subString, "!=") == 0 ||
		SubString_string_cmp(subString, ">") == 0 ||
		SubString_string_cmp(subString, "<") == 0
	) {
		return 4;
	}
	
	else if (SubString_string_cmp(subString, "+") == 0 || SubString_string_cmp(subString, "-") == 0) {
		return 5;
	}
	
	else if (SubString_string_cmp(subString, "*") == 0 || SubString_string_cmp(subString, "/") == 0 || SubString_string_cmp(subString, "%") == 0) {
		return 6;
	}
	
	else if (SubString_string_cmp(subString, "as") == 0) {
		return 7;
	}
	
	else if (SubString_string_cmp(subString, ".") == 0) {
		return 8;
	}
	
	else if (SubString_string_cmp(subString, "::") == 0) {
		return 9;
	}
	
	else {
		printf("getOperatorPrecedence error\n");
		abort();
	}
}

linkedList_Node *parseType(FileInformation *FI, linkedList_Node **current);

linkedList_Node *parseOperators(FileInformation *FI, linkedList_Node **current, linkedList_Node *left, int precedence, int ignoreEquals) {
	if (*current == NULL) {
		return left;
	}
	
	Token *operator = ((Token *)((*current)->data));
	
	if (operator->type == TokenType_operator) {
		int nextPrecedence = getOperatorPrecedence(&operator->subString);
		
		if (nextPrecedence > precedence) {
			linkedList_Node *AST = NULL;
			
			// to fix variable definitions
			if (
				ignoreEquals &&
				operator->type == TokenType_operator &&
				SubString_string_cmp(&operator->subString, "=") == 0
			) {
				return left;
			}
			
			*current = (*current)->next;
			linkedList_Node *right;
			if (SubString_string_cmp(&operator->subString, "as") == 0) {
				right = parseType(FI, current);
			} else if (SubString_string_cmp(&operator->subString, ".") == 0 || SubString_string_cmp(&operator->subString, "::") == 0) {
				right = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 1, 1), nextPrecedence, ignoreEquals);
			} else {
				right = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 1, 0), nextPrecedence, ignoreEquals);
			}
			
			ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_infixOperator));
			
			data->nodeType = ASTnodeType_infixOperator;
			data->location = operator->location;
			
			if (SubString_string_cmp(&operator->subString, "=") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_assignment;
			} else if (SubString_string_cmp(&operator->subString, "==") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_equivalent;
			} else if (SubString_string_cmp(&operator->subString, "!=") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_notEquivalent;
			} else if (SubString_string_cmp(&operator->subString, ">") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_greaterThan;
			} else if (SubString_string_cmp(&operator->subString, "<") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_lessThan;
			} else if (SubString_string_cmp(&operator->subString, "+") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_add;
			} else if (SubString_string_cmp(&operator->subString, "-") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_subtract;
			} else if (SubString_string_cmp(&operator->subString, "*") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_multiply;
			} else if (SubString_string_cmp(&operator->subString, "/") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_divide;
			} else if (SubString_string_cmp(&operator->subString, "%") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_modulo;
			} else if (SubString_string_cmp(&operator->subString, "&&") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_and;
			} else if (SubString_string_cmp(&operator->subString, "||") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_or;
			} else if (SubString_string_cmp(&operator->subString, ".") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_memberAccess;
			} else if (SubString_string_cmp(&operator->subString, "as") == 0) {
				((ASTnode_infixOperator *)data->value)->operatorType = ASTnode_infixOperatorType_cast;
			} else {
				abort();
			}
			
			((ASTnode_infixOperator *)data->value)->left = left;
			((ASTnode_infixOperator *)data->value)->right = right;
			
			return parseOperators(FI, current, AST, precedence, ignoreEquals);
		}
	}
	
	return left;
}

linkedList_Node *parseConstraintList(FileInformation *FI, linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			printf("unexpected end of file at constraint list\n");
			exit(1);
		}
		
		Token *token = (Token *)((*current)->data);
		if (token->type == TokenType_separator && SubString_string_cmp(&token->subString, "]") == 0) {
			*current = (*current)->next;
			return AST;
		}
		
		linkedList_Node *operatorAST = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 0, 0), 0, 0);
		linkedList_join(&AST, &operatorAST);
	}
}

linkedList_Node *parseType(FileInformation *FI, linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	linkedList_Node *type = parse(FI, current, ParserMode_expression, 1, 1);
	
//	linkedList_Node *stateConstructorArguments = NULL;
//	Token *token = (Token *)((*current)->data);
//	if (token->type == TokenType_operator && SubString_string_cmp(&token->subString, "<") == 0) {
//		*current = (*current)->next;
//		stateConstructorArguments = parse(FI, current, ParserMode_arguments, 1, 0);
//		Token *endToken = (Token *)((*current)->data);
//		if (endToken->type != TokenType_operator || SubString_string_cmp(&endToken->subString, ">") != 0) {
//			addStringToReportMsg("expected '>'");
//			compileError(FI, endToken->location);
//		}
//		*current = (*current)->next;
//	}
	
	linkedList_Node *constraints = NULL;
	Token *constraintToken = (Token *)((*current)->data);
	if (constraintToken->type == TokenType_separator && SubString_string_cmp(&constraintToken->subString, "[") == 0) {
		*current = (*current)->next;
		constraints = parseConstraintList(FI, current);
	}
	
	ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_constrainedType));
	data->nodeType = ASTnodeType_constrainedType;
	data->location = ((Token *)((*current)->data))->location;
	
	((ASTnode_constrainedType *)data->value)->type = type;
	((ASTnode_constrainedType *)data->value)->constraints = constraints;
	
	return AST;
}

typedef struct {
	linkedList_Node *list1;
	linkedList_Node *list2;
} linkedList_Node_tuple;

linkedList_Node_tuple parseFunctionArguments(FileInformation *FI, linkedList_Node **current) {
	linkedList_Node *argumentNames = NULL;
	linkedList_Node *argumentTypes = NULL;
	
	while (1) {
		if (*current == NULL) {
			printf("unexpected end of file at function arguments\n");
			exit(1);
		}
		
		Token *token = (Token *)((*current)->data);
		
		switch (token->type) {
			case TokenType_word: {
				*current = (*current)->next;
				endIfCurrentIsEmpty()
				Token *colon = ((Token *)((*current)->data));
				
				if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
					addStringToReportMsg("function argument expected a colon");
					compileError(FI, colon->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *type = parseType(FI, current);
				
				SubString *nameSubString = linkedList_addNode(&argumentNames, sizeof(SubString));
				nameSubString->start = token->subString.start;
				nameSubString->length = token->subString.length;
				
				// join type to argumentTypes
				linkedList_join(&argumentTypes, &type);
				
				token = ((Token *)((*current)->data));
				endIfCurrentIsEmpty()
				if (token->type != TokenType_separator || SubString_string_cmp(&token->subString, ",") != 0) {
					if (token->type == TokenType_separator) {
						if (SubString_string_cmp(&token->subString, ")") == 0) {
							return (linkedList_Node_tuple){argumentNames, argumentTypes};
						} else {
							addStringToReportMsg("unexpected separator: '");
							addSubStringToReportMsg(&token->subString);
							addStringToReportMsg("'");
							compileError(FI, token->location);
						}
					}
					addStringToReportMsg("expected a comma");
					compileError(FI, token->location);
				}
				
				break;
			}
			
			case TokenType_separator: {
				if (SubString_string_cmp(&token->subString, ")") == 0) {
					return (linkedList_Node_tuple){argumentNames, argumentTypes};
				} else {
					addStringToReportMsg("unexpected separator: '");
					addSubStringToReportMsg(&token->subString);
					addStringToReportMsg("'");
					compileError(FI, token->location);
				}
				break;
			}
			
			default: {
				addStringToReportMsg("unexpected token type inside of function arguments");
				compileError(FI, token->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
}

linkedList_Node *parse(FileInformation *FI, linkedList_Node **current, ParserMode parserMode, int returnAtNonScopeResolutionOperator, int returnAtOpeningSeparator) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		Token *token = (Token *)((*current)->data);
		
		switch (token->type) {
			case TokenType_word: {
				if (SubString_string_cmp(&token->subString, "struct") == 0) {
					*current = (*current)->next;
					
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						addStringToReportMsg("struct expected '{'");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *block = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_struct));
					
					data->nodeType = ASTnodeType_struct;
					data->location = token->location;
					
					((ASTnode_struct *)data->value)->block = block;
				}
				
				// function definition
				else if (SubString_string_cmp(&token->subString, "fn") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						addStringToReportMsg("function definition expected an openingParentheses");
						compileError(FI, openingParentheses->location);
					}
					
					*current = (*current)->next;
					linkedList_Node_tuple argument_tuple = parseFunctionArguments(FI, current);
					
					linkedList_Node *argumentNames = argument_tuple.list1;
					linkedList_Node *argumentTypes = argument_tuple.list2;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						addStringToReportMsg("function definition expected a colon");
						compileError(FI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *returnType = (ASTnode *)parseType(FI, current)->data;
					
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node *codeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = token->location;
						
						((ASTnode_function *)data->value)->external = 0;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = codeBlock;
					} else if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, ";") == 0) {
						*current = (*current)->next;
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = token->location;
						
						((ASTnode_function *)data->value)->external = 1;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = NULL;
					} else {
						addStringToReportMsg("function definition expected an openingBracket or a semicolon");
						compileError(FI, codeStart->location);
					}
				}
				
				// while loop
				else if (SubString_string_cmp(&token->subString, "while") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						addStringToReportMsg("while loop expected an openingParentheses");
						compileError(FI, openingParentheses->location);
					}
					*current = (*current)->next;
					linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
					
					endIfCurrentIsEmpty()
					Token *closingParentheses = ((Token *)((*current)->data));
					if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
						addStringToReportMsg("while loop expected ')'");
						compileError(FI, token->location);
					}
					
					*current = (*current)->next;
					
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						addStringToReportMsg("while loop expected '{'");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *codeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_while));
					
					data->nodeType = ASTnodeType_while;
					data->location = token->location;
					
					((ASTnode_while *)data->value)->expression = expression;
					((ASTnode_while *)data->value)->codeBlock = codeBlock;
				}
				
				// if statement
				else if (SubString_string_cmp(&token->subString, "if") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						addStringToReportMsg("if statement expected an openingParentheses");
						compileError(FI, openingParentheses->location);
					}
					*current = (*current)->next;
					linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
					
					endIfCurrentIsEmpty()
					Token *closingParentheses = ((Token *)((*current)->data));
					if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
						addStringToReportMsg("if statement expected ')'");
						compileError(FI, token->location);
					}
					*current = (*current)->next;
					
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						addStringToReportMsg("if statement expected '{'");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *trueCodeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_if));
					
					data->nodeType = ASTnodeType_if;
					data->location = token->location;
					
					((ASTnode_if *)data->value)->expression = expression;
					((ASTnode_if *)data->value)->trueCodeBlock = trueCodeBlock;
					
					if (*current != NULL && SubString_string_cmp(&(((Token *)((*current)->data)))->subString, "else") == 0) {
						*current = (*current)->next;
						*current = (*current)->next;
						linkedList_Node *falseCodeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
						
						((ASTnode_if *)data->value)->falseCodeBlock = falseCodeBlock;
					} else {
						((ASTnode_if *)data->value)->falseCodeBlock = NULL;
					}
				}
				
				// return statement
				else if (SubString_string_cmp(&token->subString, "return") == 0) {
					*current = (*current)->next;
					if (CURRENT_IS_NOT_SEMICOLON) {
						linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->nodeType = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = expression;
					} else {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->nodeType = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = NULL;
					}
				}
				
				// variable definition
				else if (SubString_string_cmp(&token->subString, "var") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					if (nameToken->type != TokenType_word) {
						addStringToReportMsg("expected word after var keyword");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						addStringToReportMsg("variable definition expected a colon");
						compileError(FI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *type = (ASTnode *)parseType(FI, current)->data;
					
					linkedList_Node *expression = NULL;
					
					Token *equals = ((Token *)((*current)->data));
					if (equals->type == TokenType_operator && SubString_string_cmp(&equals->subString, "=") == 0) {
						*current = (*current)->next;
						expression = parse(FI, current, ParserMode_expression, 0, 0);
					}
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variableDefinition));
					
					data->nodeType = ASTnodeType_variableDefinition;
					data->location = token->location;
					
					((ASTnode_variableDefinition *)data->value)->name = &nameToken->subString;
					((ASTnode_variableDefinition *)data->value)->type = type;
					((ASTnode_variableDefinition *)data->value)->expression = expression;
				}
				
				// constant definition
				else if (SubString_string_cmp(&token->subString, "const") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					if (nameToken->type != TokenType_word) {
						addStringToReportMsg("expected word after const keyword");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						addStringToReportMsg("constant definition expected a colon");
						compileError(FI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *type = (ASTnode *)parseType(FI, current)->data;
					
					linkedList_Node *expression = NULL;
					
					Token *equals = ((Token *)((*current)->data));
					if (equals->type == TokenType_operator && SubString_string_cmp(&equals->subString, "=") == 0) {
						*current = (*current)->next;
						expression = parse(FI, current, ParserMode_expression, 0, 0);
					}
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_constantDefinition));
					
					data->nodeType = ASTnodeType_constantDefinition;
					data->location = token->location;
					
					((ASTnode_constantDefinition *)data->value)->name = &nameToken->subString;
					((ASTnode_constantDefinition *)data->value)->type = type;
					((ASTnode_constantDefinition *)data->value)->expression = expression;
				}
				
				// true
				else if (SubString_string_cmp(&token->subString, "true") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_bool));
					data->nodeType = ASTnodeType_bool;
					data->location = token->location;
					
					((ASTnode_bool *)data->value)->isTrue = 1;
					
					*current = (*current)->next;
				}
				
				// false
				else if (SubString_string_cmp(&token->subString, "false") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_bool));
					data->nodeType = ASTnodeType_bool;
					data->location = token->location;
					
					((ASTnode_bool *)data->value)->isTrue = 0;
					
					*current = (*current)->next;
				}
				
				// identifier
				else {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_identifier));
					data->nodeType = ASTnodeType_identifier;
					data->location = token->location;
					((ASTnode_identifier *)data->value)->name = &token->subString;
					
					*current = (*current)->next;
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				data->nodeType = ASTnodeType_number;
				data->location = token->location;
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(FI, current).value;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_ellipsis: {
				addStringToReportMsg("unexpected ellipsis");
				compileError(FI, token->location);
				break;
			}
			
			case TokenType_separator: {
				if (SubString_string_cmp(&token->subString, "(") == 0) {
					if (returnAtOpeningSeparator) {
						return AST;
					}
					
					int forFunction = 1;
					
					if (AST != NULL) {
						linkedList_Node *last = linkedList_getLast(AST);
						if (
							((ASTnode *)last->data)->nodeType == ASTnodeType_identifier ||
							((ASTnode *)last->data)->nodeType == ASTnodeType_infixOperator
						) {
							forFunction = 1;
						} else {
							forFunction = 0;
						}
					} else {
						forFunction = 0;
					}
					
					if (forFunction) {
						linkedList_Node *left = linkedList_popLast(&AST);
						ASTnode *leftNode = (ASTnode *)left->data;
						
						*current = (*current)->next;
						linkedList_Node *arguments = parse(FI, current, ParserMode_arguments, 0, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
						
						data->nodeType = ASTnodeType_call;
						data->location = leftNode->location;
						
						((ASTnode_call *)data->value)->builtin = 0;
						((ASTnode_call *)data->value)->left = left;
						((ASTnode_call *)data->value)->arguments = arguments;
					} else {
						*current = (*current)->next;
						linkedList_Node *newAST = parse(FI, current, ParserMode_expression, 0, 0);
						linkedList_join(&AST, &newAST);
						Token *closingParentheses = ((Token *)((*current)->data));
						if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
							addStringToReportMsg("expected closingParentheses");
							compileError(FI, closingParentheses->location);
						}
						*current = (*current)->next;
					}
				} else if (SubString_string_cmp(&token->subString, "[") == 0) {
					if (returnAtOpeningSeparator) {
						return AST;
					}
					
					linkedList_Node *left = linkedList_popLast(&AST);
					ASTnode *leftNode = (ASTnode *)left->data;
					
					*current = (*current)->next;
					linkedList_Node *right = parse(FI, current, ParserMode_expression, 0, 0);
					Token *closingBracket = ((Token *)((*current)->data));
					if (closingBracket->type != TokenType_separator || SubString_string_cmp(&closingBracket->subString, "]") != 0) {
						addStringToReportMsg("subscript expected a closingBracket");
						compileError(FI, closingBracket->location);
					}
					*current = (*current)->next;
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_subscript));
					
					data->nodeType = ASTnodeType_subscript;
					data->location = leftNode->location;
					
					((ASTnode_subscript *)data->value)->left = left;
					((ASTnode_subscript *)data->value)->right = right;
				} else if (
					SubString_string_cmp(&token->subString, ")") == 0 ||
					SubString_string_cmp(&token->subString, "}") == 0 ||
					SubString_string_cmp(&token->subString, "]") == 0
				) {
					if (parserMode != ParserMode_expression) {
						*current = (*current)->next;
					}
					return AST;
				} else if (parserMode != ParserMode_expression) {
					addStringToReportMsg("unexpected separator: '");
					addSubStringToReportMsg(&token->subString);
					addStringToReportMsg("'");
					compileError(FI, token->location);
				}
				break;
			}
			
			case TokenType_operator: {
				if (returnAtNonScopeResolutionOperator && SubString_string_cmp(&token->subString, "::") != 0) {
					return AST;
				}
				linkedList_Node *left = linkedList_popLast(&AST);
				linkedList_Node *operatorAST = parseOperators(FI, current, left, 0, 0);
				linkedList_join(&AST, &operatorAST);
				break;
			}
			
			case TokenType_string: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_string));
				
				data->nodeType = ASTnodeType_string;
				data->location = token->location;
				((ASTnode_string *)data->value)->value = &token->subString;
				
				*current = (*current)->next;
				break;
			}
			
			case TokenType_selfReference: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
				
				data->nodeType = ASTnodeType_selfReference;
				data->location = token->location;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_builtinIndicator: {
				*current = (*current)->next;
				
				linkedList_Node *left = parse(FI, current, ParserMode_expression, 0, 1);
				ASTnode *leftNode = (ASTnode *)left->data;
				
				*current = (*current)->next;
				linkedList_Node *arguments = parse(FI, current, ParserMode_arguments, 0, 0);
				
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
				
				data->nodeType = ASTnodeType_call;
				data->location = leftNode->location;
				
				((ASTnode_call *)data->value)->builtin = 1;
				((ASTnode_call *)data->value)->left = left;
				((ASTnode_call *)data->value)->arguments = arguments;
				break;
			}
			
			default: {
				printf("unknown token type: %u\n", token->type);
				exit(1);
				break;
			}
		}
		
		ASTnode *lastNode = (ASTnode *)linkedList_getLast(AST)->data;
		
		if (*current == NULL) {
			if (getIfNodeShouldHaveSemicolon(lastNode)) {
				addStringToReportMsg("expected ';' after statement, but the file ended");
				compileError(FI, lastNode->location);
			}
			return AST;
		}
		
		if (!returnAtNonScopeResolutionOperator && ((Token *)((*current)->data))->type == TokenType_operator) {
			continue;
		}
		
		if (
			((Token *)((*current)->data))->type == TokenType_separator &&
			SubString_string_cmp(&((Token *)((*current)->data))->subString, ")") == 0
		) {
			continue;
		}
		
		if (
			!returnAtOpeningSeparator &&
			((Token *)((*current)->data))->type == TokenType_separator &&
			(
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "(") == 0 ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "[") == 0
			)
		) {
			continue;
		}
		
		if (parserMode == ParserMode_arguments) {
			if (
				((Token *)((*current)->data))->type != TokenType_separator ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, ",") != 0
			) {
				addStringToReportMsg("expected ',' inside of argument list");
				compileError(FI, lastNode->location);
			}
			*current = (*current)->next;
		}
		
		int nextTokenWillMoveLastNode = ((Token *)((*current)->data))->type == TokenType_operator ||
		(
			((Token *)((*current)->data))->type == TokenType_separator &&
			(
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "(") == 0 ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "[") == 0
			)
		);
		
		// if the node that was just generated should have a semicolon
		if (parserMode == ParserMode_codeBlock && !nextTokenWillMoveLastNode && getIfNodeShouldHaveSemicolon(lastNode)) {
			if (CURRENT_IS_NOT_SEMICOLON) {
				addStringToReportMsg("expected ';' after statement");
				compileError(FI, lastNode->location);
			}
			*current = (*current)->next;
		}
		
		if (!returnAtNonScopeResolutionOperator && nextTokenWillMoveLastNode) {
			continue;
		}
		
		if (parserMode == ParserMode_expression) {
			return AST;
		}
	}
}
