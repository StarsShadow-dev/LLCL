/*
 This file has the buildLLVM function and a bunch of functions that the BuildLLVM function calls.
 
 This file is: very big, hard to read, very hard to debug.
 Part of the debuging issue comes from how much casting is being used.
 But I do not think there is much I can do about that in C.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "globals.h"
#include "builder.h"
#include "error.h"
#include "utilities.h"

void addDILocation(CharAccumulator *source, int ID, SourceLocation location) {
	CharAccumulator_appendChars(source, ", !dbg !DILocation(line: ");
	CharAccumulator_appendInt(source, location.line);
	CharAccumulator_appendChars(source, ", column: ");
	CharAccumulator_appendInt(source, location.columnStart + 1);
	CharAccumulator_appendChars(source, ", scope: !");
	CharAccumulator_appendInt(source, ID);
	CharAccumulator_appendChars(source, ")");
}

void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *data = linkedList_addNode(context, sizeof(ContextBinding) + sizeof(ContextBinding_simpleType));
	
	data->key = key;
	data->type = ContextBindingType_simpleType;
	data->byteSize = byteSize;
	data->byteAlign = byteAlign;
	((ContextBinding_simpleType *)data->value)->LLVMtype = LLVMtype;
}

ContextBinding *getContextBindingFromString(GlobalBuilderInformation *GBI, char *key) {
	int index = GBI->level;
	while (index >= 0) {
		linkedList_Node *current = GBI->context[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_string_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	return NULL;
}

ContextBinding *getContextBindingFromSubString(GlobalBuilderInformation *GBI, SubString *key) {
	int index = GBI->level;
	while (index >= 0) {
		linkedList_Node *current = GBI->context[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_SubString_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	return NULL;
}

/// if the node is a memberAccess operator the function calls itself until it gets to the end of the memberAccess operators
ContextBinding *getContextBindingFromVariableNode(GlobalBuilderInformation *GBI, ASTnode *node) {
	if (node->nodeType == ASTnodeType_variable) {
		ASTnode_variable *data = (ASTnode_variable *)node->value;
		
		return getContextBindingFromSubString(GBI, data->name);
	} else if (node->nodeType == ASTnodeType_operator) {
		ASTnode_operator *data = (ASTnode_operator *)node->value;
		
		if (data->operatorType != ASTnode_operatorType_memberAccess) abort();
		
		// more recursion!
		return getContextBindingFromVariableNode(GBI, (ASTnode *)data->left->data);
	} else {
		abort();
	}
	
	return NULL;
}

char *getLLVMtypeFromASTnode(GlobalBuilderInformation *GBI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_type) abort();
	
	ContextBinding *binding = getContextBindingFromSubString(GBI, ((ASTnode_type *)node->value)->name);
	
	if (binding->type == ContextBindingType_simpleType) {
		return ((ContextBinding_simpleType *)binding->value)->LLVMtype;
	} else if (binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)binding->value)->LLVMname;
	}
	
	abort();
}

char *getLLVMtypeFromBuilderType(GlobalBuilderInformation *GBI, BuilderType *type) {
	if (type->binding->type == ContextBindingType_simpleType) {
		return ((ContextBinding_simpleType *)type->binding->value)->LLVMtype;
	} else if (type->binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)type->binding->value)->LLVMname;
	}
	
	abort();
}

int ifTypeIsNamed(BuilderType *expectedType, char *actualTypeString) {
	if (SubString_string_cmp(expectedType->binding->key, actualTypeString) != 0) {
		return 1;
	}
	
	return 0;
}

SubString *getTypeAsSubString(BuilderType *type) {
	return type->binding->key;
}

void addTypeFromString(GlobalBuilderInformation *GBI, linkedList_Node **list, char *string) {
	ContextBinding *binding = getContextBindingFromString(GBI, string);
	if (binding == NULL) abort();

	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = binding;
}

void addTypeFromBuilderType(GlobalBuilderInformation *GBI, linkedList_Node **list, BuilderType *type) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = type->binding;
}

void addTypeFromBinding(GlobalBuilderInformation *GBI, linkedList_Node **list, ContextBinding *binding) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = binding;
}

linkedList_Node *typeToList(BuilderType type) {
	linkedList_Node *list = NULL;
	
	BuilderType *data = linkedList_addNode(&list, sizeof(BuilderType));
	data->binding = type.binding;
	
	return list;
}

void expectSameType(BuilderType *expectedType, BuilderType actualType, SourceLocation location) {
	if (expectedType->binding != actualType.binding) {
		addStringToErrorMsg("unexpected type");
		
		addStringToErrorIndicator("expected type '");
		addSubStringToErrorIndicator(expectedType->binding->key);
		addStringToErrorIndicator("' but got type '");
		addSubStringToErrorIndicator(actualType.binding->key);
		addStringToErrorIndicator("'");
		compileError(location);
	}
}

void expectUnusedMemberBindingName(ContextBinding_struct *structData, SubString *name, SourceLocation location) {
	linkedList_Node *currentMemberBinding = structData->memberBindings;
	while (currentMemberBinding != NULL) {
		ContextBinding *memberBinding = (ContextBinding *)currentMemberBinding->data;
		
		if (SubString_SubString_cmp(memberBinding->key, name) == 0) {
			addStringToErrorMsg("the name '");
			addSubStringToErrorMsg(name);
			addStringToErrorMsg("' is defined multiple times inside a struct");
			
			addStringToErrorIndicator("'");
			addSubStringToErrorIndicator(name);
			addStringToErrorIndicator("' redefined here");
			compileError(location);
		}
		
		currentMemberBinding = currentMemberBinding->next;
	}
}

void expectUnusedName(GlobalBuilderInformation *GBI, SubString *name, SourceLocation location) {
	ContextBinding *binding = getContextBindingFromSubString(GBI, name);
	if (binding != NULL) {
		addStringToErrorMsg("the name '");
		addSubStringToErrorMsg(name);
		addStringToErrorMsg("' is defined multiple times");
		
		addStringToErrorIndicator("'");
		addSubStringToErrorIndicator(name);
		addStringToErrorIndicator("' redefined here");
		compileError(location);
	}
}

ContextBinding *expectTypeExists(GlobalBuilderInformation *GBI, SubString *name, SourceLocation location) {
	ContextBinding *binding = getContextBindingFromSubString(GBI, name);
	if (binding == NULL) {
		addStringToErrorMsg("use of undeclared type");
		
		addStringToErrorIndicator("nothing named '");
		addSubStringToErrorIndicator(name);
		addStringToErrorIndicator("'");
		compileError(location);
	}
	
	if (binding->type != ContextBindingType_simpleType && binding->type != ContextBindingType_struct) {
		if (binding->type == ContextBindingType_function) {
			addStringToErrorMsg("expected type but got a function");
		} else {
			abort();
		}
		
		addStringToErrorIndicator("'");
		addSubStringToErrorIndicator(name);
		addStringToErrorIndicator("' is not a type");
		compileError(location);
	}
	
	return binding;
}

BuilderType getTypeFromASTnode(GlobalBuilderInformation *GBI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_type) abort();
	
	ContextBinding *binding = expectTypeExists(GBI, ((ASTnode_type *)node->value)->name, node->location);
	
	return (BuilderType){binding};
}

BuilderType getTypeFromBinding(ContextBinding *binding) {
	return (BuilderType){binding};
}

void generateFunction(GlobalBuilderInformation *GBI, CharAccumulator *outerSource, ContextBinding_function *function, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	if (data->external) {
		CharAccumulator_appendSubString(outerSource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->value);
	}
	
	else {
		char *LLVMreturnType = getLLVMtypeFromASTnode(GBI, data->returnType);
		
		CharAccumulator_appendChars(outerSource, "\n\ndefine ");
		CharAccumulator_appendChars(outerSource, LLVMreturnType);
		CharAccumulator_appendChars(outerSource, " @");
		CharAccumulator_appendChars(outerSource, function->LLVMname);
		CharAccumulator_appendChars(outerSource, "(");
		
		CharAccumulator functionSource = {100, 0, 0};
		CharAccumulator_initialize(&functionSource);
		
		linkedList_Node *currentArgumentType = data->argumentTypes;
		linkedList_Node *currentArgumentName = data->argumentNames;
		if (currentArgumentType != NULL) {
			int argumentCount =  linkedList_getCount(&data->argumentTypes);
			while (1) {
				ContextBinding *argumentTypeBinding = getContextBindingFromSubString(GBI, ((ASTnode_type *)((ASTnode *)currentArgumentType->data)->value)->name);
				
				char *currentArgumentLLVMtype = getLLVMtypeFromASTnode(GBI, (ASTnode *)currentArgumentType->data);
				CharAccumulator_appendChars(outerSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(outerSource, " %");
				CharAccumulator_appendInt(outerSource, function->registerCount);
				
				CharAccumulator_appendChars(&functionSource, "\n\t%");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, " = alloca ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, " %");
				CharAccumulator_appendInt(&functionSource, function->registerCount);
				CharAccumulator_appendChars(&functionSource, ", ptr %");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				expectUnusedName(GBI, (SubString *)currentArgumentName->data, node->location);
				
				BuilderType type = getTypeFromASTnode(GBI, (ASTnode *)currentArgumentType->data);
				
				ContextBinding *argumentVariableData = linkedList_addNode(&GBI->context[GBI->level + 1], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				argumentVariableData->key = (SubString *)currentArgumentName->data;
				argumentVariableData->type = ContextBindingType_variable;
				argumentVariableData->byteSize = argumentTypeBinding->byteSize;
				argumentVariableData->byteAlign = argumentTypeBinding->byteAlign;
				
				((ContextBinding_variable *)argumentVariableData->value)->LLVMRegister = function->registerCount + argumentCount + 1;
				((ContextBinding_variable *)argumentVariableData->value)->LLVMtype = currentArgumentLLVMtype;
				((ContextBinding_variable *)argumentVariableData->value)->type = type;
				
				function->registerCount++;
				
				if (currentArgumentType->next == NULL) {
					break;
				}
				
				CharAccumulator_appendChars(outerSource, ", ");
				currentArgumentType = currentArgumentType->next;
				currentArgumentName = currentArgumentName->next;
			}
			
			function->registerCount += argumentCount;
		}
		
		function->registerCount++;
		
		CharAccumulator_appendChars(outerSource, ")");
		if (compilerOptions.includeDebugInformation) {
			CharAccumulator_appendChars(outerSource, " !dbg !");
			CharAccumulator_appendInt(outerSource, function->debugInformationScopeID);
		}
		CharAccumulator_appendChars(outerSource, " {");
		CharAccumulator_appendChars(outerSource, functionSource.data);
		int functionHasReturned = buildLLVM(GBI, function, outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
		
		if (!functionHasReturned) {
			addStringToErrorMsg("function did not return");
			
			addStringToErrorIndicator("the compiler cannot guarantee that function '");
			addSubStringToErrorIndicator(data->name);
			addStringToErrorIndicator("' returns");
			compileError(node->location);
		}
		
		CharAccumulator_appendChars(outerSource, "\n}");
		
		CharAccumulator_free(&functionSource);
	}
}

int buildLLVM(GlobalBuilderInformation *GBI, ContextBinding_function *outerFunction, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int loadVariables, int withTypes, int withCommas) {
	GBI->level++;
	if (GBI->level > maxVariablesLevel) {
		printf("level (%i) > maxVariablesLevel (%i)\n", GBI->level, maxVariablesLevel);
		abort();
	}
	
	linkedList_Node **currentExpectedType = NULL;
	if (expectedTypes != NULL) {
		currentExpectedType = &expectedTypes;
	}
	
	int hasReturned = 0;
	
	//
	// pre-loop
	//
	
	if (GBI->level == 0) {
		linkedList_Node *preLoopCurrent = current;
		while (preLoopCurrent != NULL) {
			ASTnode *node = ((ASTnode *)preLoopCurrent->data);
			
			if (node->nodeType == ASTnodeType_function) {
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				// make sure that the name is not already used
				expectUnusedName(GBI, data->name, node->location);
				
				// make sure that the return type actually exists
				expectTypeExists(GBI, ((ASTnode_type *)data->returnType->value)->name, data->returnType->location);
				
				linkedList_Node *argumentTypes = NULL;
				
				// make sure that the types of all arguments actually exist
				linkedList_Node *currentArgument = data->argumentTypes;
				while (currentArgument != NULL) {
					ContextBinding *currentArgumentBinding = expectTypeExists(GBI, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name, ((ASTnode *)currentArgument->data)->location);
					
					BuilderType *data = linkedList_addNode(&argumentTypes, sizeof(BuilderType));
					data->binding = currentArgumentBinding;
					
					currentArgument = currentArgument->next;
				}
				
				char *LLVMreturnType = getLLVMtypeFromASTnode(GBI, data->returnType);
				
				BuilderType returnType = getTypeFromASTnode(GBI, data->returnType);
				
				ContextBinding *functionData = linkedList_addNode(&GBI->context[GBI->level], sizeof(ContextBinding) + sizeof(ContextBinding_function));
				
				char *functionLLVMname = safeMalloc(data->name->length + 1);
				memcpy(functionLLVMname, data->name->start, data->name->length);
				functionLLVMname[data->name->length] = 0;
				
				functionData->key = data->name;
				functionData->type = ContextBindingType_function;
				// I do not think I need to set byteSize or byteAlign to anything specific
				functionData->byteSize = 0;
				functionData->byteAlign = 0;
				
				((ContextBinding_function *)functionData->value)->LLVMname = functionLLVMname;
				((ContextBinding_function *)functionData->value)->LLVMreturnType = LLVMreturnType;
				((ContextBinding_function *)functionData->value)->argumentTypes = argumentTypes;
				((ContextBinding_function *)functionData->value)->returnType = returnType;
				((ContextBinding_function *)functionData->value)->registerCount = 0;
				
				if (compilerOptions.includeDebugInformation) {
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, "\n!");
					CharAccumulator_appendInt(GBI->LLVMmetadataSource, GBI->metadataCount);
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, " = distinct !DISubprogram(");
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, "name: \"");
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, functionLLVMname);
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, "\", scope: !");
					CharAccumulator_appendInt(GBI->LLVMmetadataSource, GBI->debugInformationFileScopeID);
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, ", file: !");
					CharAccumulator_appendInt(GBI->LLVMmetadataSource, GBI->debugInformationFileScopeID);
					// so apparently Homebrew crashes without a helpful error message if "type" is not here
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, ", type: !DISubroutineType(types: !{}), unit: !");
					CharAccumulator_appendInt(GBI->LLVMmetadataSource, GBI->debugInformationCompileUnitID);
					CharAccumulator_appendChars(GBI->LLVMmetadataSource, ")");
					
					((ContextBinding_function *)functionData->value)->debugInformationScopeID = GBI->metadataCount;
					
					GBI->metadataCount++;
				}
			}
			
			else if (node->nodeType == ASTnodeType_struct) {
				ASTnode_struct *data = (ASTnode_struct *)node->value;
				
				ContextBinding *structBinding = linkedList_addNode(&GBI->context[GBI->level], sizeof(ContextBinding) + sizeof(ContextBinding_struct));
				structBinding->key = data->name;
				structBinding->type = ContextBindingType_struct;
				structBinding->byteSize = 0;
				// 4 by default but set it to pointer_byteSize if there is a pointer is in the struct
				structBinding->byteAlign = 4;
				
				int strlength = strlen("%struct.");
				
				char *LLVMname = safeMalloc(strlength + data->name->length + 1);
				memcpy(LLVMname, "%struct.", strlength);
				memcpy(LLVMname + strlength, data->name->start, data->name->length);
				LLVMname[strlength + data->name->length] = 0;
				
				((ContextBinding_struct *)structBinding->value)->LLVMname = LLVMname;
				((ContextBinding_struct *)structBinding->value)->memberNames = NULL;
				((ContextBinding_struct *)structBinding->value)->memberBindings = NULL;
				
				CharAccumulator_appendChars(outerSource, "\n\n%struct.");
				CharAccumulator_appendSubString(outerSource, data->name);
				CharAccumulator_appendChars(outerSource, " = type { ");
				
				int addComma = 0;
				
				linkedList_Node *currentMember = data->block;
				while (currentMember != NULL) {
					ASTnode *node = ((ASTnode *)currentMember->data);
					
					if (node->nodeType == ASTnodeType_variableDefinition) {
						ASTnode_variableDefinition *memberData = (ASTnode_variableDefinition *)node->value;
						
						expectUnusedMemberBindingName((ContextBinding_struct *)structBinding->value, memberData->name, node->location);
						
						char *LLVMtype = getLLVMtypeFromASTnode(GBI, memberData->type);
						
						ContextBinding *typeBinding = expectTypeExists(GBI, ((ASTnode_type *)memberData->type->value)->name, memberData->type->location);
						
						// if there is a pointer anywhere in the struct then the struct should be aligned by pointer_byteSize
						if (strcmp(LLVMtype, "ptr") == 0) {
							structBinding->byteAlign = pointer_byteSize;
						}
						
						structBinding->byteSize += typeBinding->byteSize;
						
						if (addComma) {
							CharAccumulator_appendChars(outerSource, ", ");
						}
						CharAccumulator_appendChars(outerSource, LLVMtype);
						
						SubString *nameData = linkedList_addNode(&((ContextBinding_struct *)structBinding->value)->memberNames, sizeof(SubString));
						memcpy(nameData, memberData->name, sizeof(SubString));
						
						BuilderType type = getTypeFromASTnode(GBI, memberData->type);
						
						ContextBinding *variableBinding = linkedList_addNode(&((ContextBinding_struct *)structBinding->value)->memberBindings, sizeof(ContextBinding) + sizeof(ContextBinding_variable));
						
						variableBinding->key = memberData->name;
						variableBinding->type = ContextBindingType_variable;
						variableBinding->byteSize = typeBinding->byteSize;
						variableBinding->byteAlign = typeBinding->byteSize;
						
						// TODO: LLVMRegister
						((ContextBinding_variable *)variableBinding->value)->LLVMRegister = 0;
						((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
						((ContextBinding_variable *)variableBinding->value)->type = type;
						
						addComma = 1;
					} else if (node->nodeType == ASTnodeType_function) {
						ASTnode_function *memberData = (ASTnode_function *)node->value;
						
						expectUnusedMemberBindingName((ContextBinding_struct *)structBinding->value, memberData->name, node->location);
						
						// make sure that the return type actually exists
						expectTypeExists(GBI, ((ASTnode_type *)memberData->returnType->value)->name, memberData->returnType->location);
						
						// make sure that the types of all arguments actually exist
						linkedList_Node *currentArgument = memberData->argumentTypes;
						while (currentArgument != NULL) {
							expectTypeExists(GBI, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name, ((ASTnode *)currentArgument->data)->location);
							
							currentArgument = currentArgument->next;
						}
						
						char *LLVMreturnType = getLLVMtypeFromASTnode(GBI, memberData->returnType);
						
						SubString *nameData = linkedList_addNode(&((ContextBinding_struct *)structBinding->value)->memberNames, sizeof(SubString));
						memcpy(nameData, memberData->name, sizeof(SubString));
						
						int functionLLVMnameLength = data->name->length + 1 + memberData->name->length;
						char *functionLLVMname = safeMalloc(functionLLVMnameLength + 1);
						snprintf(functionLLVMname, functionLLVMnameLength + 1, "%.*s.%s", data->name->length, data->name->start, memberData->name->start);
						
						BuilderType returnType = getTypeFromASTnode(GBI, memberData->returnType);
						
						ContextBinding *functionData = linkedList_addNode(&((ContextBinding_struct *)structBinding->value)->memberBindings, sizeof(ContextBinding) + sizeof(ContextBinding_function));
						
						functionData->key = memberData->name;
						functionData->type = ContextBindingType_function;
						functionData->byteSize = pointer_byteSize;
						functionData->byteAlign = pointer_byteSize;
						
						((ContextBinding_function *)functionData->value)->LLVMname = functionLLVMname;
						((ContextBinding_function *)functionData->value)->LLVMreturnType = LLVMreturnType;
						((ContextBinding_function *)functionData->value)->argumentTypes = memberData->argumentTypes;
						((ContextBinding_function *)functionData->value)->returnType = returnType;
						((ContextBinding_function *)functionData->value)->registerCount = 0;
					} else {
						abort();
					}
					
					currentMember = currentMember->next;
				}
				
				CharAccumulator_appendChars(outerSource, " }");
			}
			
			preLoopCurrent = preLoopCurrent->next;
		}
	}
	
	//
	// main loop
	//
	
	while (current != NULL) {
		ASTnode *node = ((ASTnode *)current->data);
		
		switch (node->nodeType) {
			case ASTnodeType_function: {
				if (GBI->level != 0) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				ContextBinding *functionBinding = getContextBindingFromSubString(GBI, data->name);
				if (functionBinding == NULL || functionBinding->type != ContextBindingType_function) abort();
				ContextBinding_function *function = (ContextBinding_function *)functionBinding->value;
				
				generateFunction(GBI, outerSource, function, node);
				
				break;
			}
			
			case ASTnodeType_struct: {
				if (GBI->level != 0) {
					printf("struct definitions are only allowed at top level\n");
					compileError(node->location);
				}
				
				ASTnode_struct *data = (ASTnode_struct *)node->value;
				
				ContextBinding *structBinding = getContextBindingFromSubString(GBI, data->name);
				if (structBinding == NULL || structBinding->type != ContextBindingType_struct) abort();
				ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
				
				linkedList_Node *currentMember = data->block;
				linkedList_Node *currentMemberBinding = structData->memberBindings;
				while (currentMember != NULL) {
					ASTnode *memberNode = ((ASTnode *)currentMember->data);
					ContextBinding *memberBinding = (ContextBinding *)currentMemberBinding->data;
					
					if (memberBinding->type == ContextBindingType_function) {
						generateFunction(GBI, outerSource, (ContextBinding_function *)memberBinding->value, memberNode);
					}
					
					currentMember = currentMember->next;
					currentMemberBinding = currentMemberBinding->next;
				}
				
				break;
			}
			
			case ASTnodeType_call: {
				if (outerFunction == NULL) {
					printf("function calls are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				CharAccumulator leftSource = {100, 0, 0};
				CharAccumulator_initialize(&leftSource);
				
				linkedList_Node *leftTypes = NULL;
				buildLLVM(GBI, outerFunction, outerSource, &leftSource, NULL, &leftTypes, data->left, 1, 0, 0);
				
				ContextBinding *functionToCallBinding = ((BuilderType *)leftTypes->data)->binding;
				ContextBinding_function *functionToCall = (ContextBinding_function *)functionToCallBinding->value;
				
				if (types != NULL) {
					addTypeFromBuilderType(GBI, types, &functionToCall->returnType);
					break;
				}
				
				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
				int actualArgumentCount = linkedList_getCount(&data->arguments);
				
				if (expectedArgumentCount > actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" did not get enough arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				if (expectedArgumentCount < actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" got too many arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				
				if (expectedTypes != NULL) {
					expectSameType((BuilderType *)expectedTypes->data, functionToCall->returnType, node->location);
				}
				
				if (types == NULL) {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(GBI, outerFunction, outerSource, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1, 1);
					
					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						CharAccumulator_appendChars(outerSource, " = call ");
						
						if (innerSource != NULL) {
							if (withTypes) {
								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
								CharAccumulator_appendChars(innerSource, " ");
							}
							CharAccumulator_appendChars(innerSource, "%");
							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
						}
						
						outerFunction->registerCount++;
					} else {
						CharAccumulator_appendChars(outerSource, "\n\tcall ");
					}
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(outerSource, " @");
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
					CharAccumulator_appendChars(outerSource, "(");
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					CharAccumulator_appendChars(outerSource, ")");
					
					if (compilerOptions.includeDebugInformation) {
						addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
					}
					
					CharAccumulator_free(&newInnerSource);
				}
				
				break;
			}
				
			case ASTnodeType_while: {
				if (outerFunction == NULL) {
					printf("while loops are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_while *data = (ASTnode_while *)node->value;
				
				linkedList_Node *expectedTypesForWhile = NULL;
				addTypeFromString(GBI, &expectedTypesForWhile, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump1_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump1_outerSource);
				buildLLVM(GBI, outerFunction, &jump1_outerSource, &expressionSource, expectedTypesForWhile, NULL, data->expression, 1, 1, 0);
				
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump2_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump2_outerSource);
				buildLLVM(GBI, outerFunction, &jump2_outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
				
				int endJump = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump1_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, endJump);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump2_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				CharAccumulator_free(&jump1_outerSource);
				CharAccumulator_free(&jump2_outerSource);
				
				break;
			}
			
			case ASTnodeType_if: {
				if (outerFunction == NULL) {
					printf("if statements are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addTypeFromString(GBI, &expectedTypesForIf, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(GBI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, NULL, data->expression, 1, 1, 0);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator trueCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&trueCodeBlockSource);
				int trueHasReturned = buildLLVM(GBI, outerFunction, &trueCodeBlockSource, NULL, NULL, NULL, data->trueCodeBlock, 0, 0, 0);
				CharAccumulator falseCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&falseCodeBlockSource);
				
				int endJump;
				
				if (data->falseCodeBlock == NULL) {
					endJump = outerFunction->registerCount;
					outerFunction->registerCount += 2;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
				} else {
					int jump2 = outerFunction->registerCount;
					outerFunction->registerCount++;
					int falseHasReturned = buildLLVM(GBI, outerFunction, &falseCodeBlockSource, NULL, NULL, NULL, data->falseCodeBlock, 0, 0, 0);
					
					endJump = outerFunction->registerCount;
					outerFunction->registerCount++;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump2);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump2);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, falseCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					if (trueHasReturned && falseHasReturned) {
//						hasReturned = 1;
					}
				}
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				
				CharAccumulator_free(&trueCodeBlockSource);
				CharAccumulator_free(&falseCodeBlockSource);
				
				break;
			}
			
			case ASTnodeType_return: {
				if (outerFunction == NULL) {
					printf("return statements are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					if (ifTypeIsNamed(&outerFunction->returnType, "Void")) {
						printf("Returning Void in a function that does not return Void.\n");
						compileError(node->location);
					}
					CharAccumulator_appendChars(outerSource, "\n\tret void");
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(GBI, outerFunction, outerSource, &newInnerSource, typeToList(outerFunction->returnType), NULL, data->expression, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					
					CharAccumulator_free(&newInnerSource);
				}
				
				if (compilerOptions.includeDebugInformation) {
					addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
				}
				
				hasReturned = 1;
				
				outerFunction->registerCount++;
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerFunction == NULL) {
					printf("variable definitions are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				expectUnusedName(GBI, data->name, node->location);
				
				ContextBinding* typeBinding = expectTypeExists(GBI, ((ASTnode_type *)data->type->value)->name, data->type->location);
				
				char *LLVMtype = getLLVMtypeFromASTnode(GBI, data->type);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(GBI, outerFunction, outerSource, &expressionSource, typeToList(getTypeFromBinding(typeBinding)), NULL, data->expression, 1, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = alloca ");
				CharAccumulator_appendChars(outerSource, LLVMtype);
				CharAccumulator_appendChars(outerSource, ", align ");
				CharAccumulator_appendInt(outerSource, typeBinding->byteAlign);
				
				if (data->expression != NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr %");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, typeBinding->byteAlign);
				}
				
				BuilderType type = getTypeFromASTnode(GBI, data->type);
				
				ContextBinding *variableData = linkedList_addNode(&GBI->context[GBI->level], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				variableData->key = data->name;
				variableData->type = ContextBindingType_variable;
				variableData->byteSize = pointer_byteSize;
				variableData->byteAlign = pointer_byteSize;
				
				((ContextBinding_variable *)variableData->value)->LLVMRegister = outerFunction->registerCount;
				((ContextBinding_variable *)variableData->value)->LLVMtype = LLVMtype;
				((ContextBinding_variable *)variableData->value)->type = type;
				
				outerFunction->registerCount++;
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_operator: {
				ASTnode_operator *data = (ASTnode_operator *)node->value;
				
				CharAccumulator leftInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&leftInnerSource);
				
				CharAccumulator rightInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&rightInnerSource);
				
				if (data->operatorType == ASTnode_operatorType_assignment) {
					if (outerFunction == NULL) {
						printf("variable assignments are only allowed in a function\n");
						compileError(node->location);
					}
					
					ContextBinding *leftVariable = getContextBindingFromVariableNode(GBI, (ASTnode *)data->left->data);
					
					CharAccumulator leftSource = {100, 0, 0};
					CharAccumulator_initialize(&leftSource);
					linkedList_Node *leftType = NULL;
					buildLLVM(GBI, outerFunction, outerSource, &leftSource, NULL, &leftType, data->left, 0, 0, 0);
					
					CharAccumulator rightSource = {100, 0, 0};
					CharAccumulator_initialize(&rightSource);
					buildLLVM(GBI, outerFunction, outerSource, &rightSource, leftType, NULL, data->right, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, rightSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr ");
					CharAccumulator_appendChars(outerSource, leftSource.data);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, leftVariable->byteAlign);
					
					CharAccumulator_free(&rightSource);
					CharAccumulator_free(&leftSource);
					
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_memberAccess) {
					ASTnode *leftNode = (ASTnode *)data->left->data;
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					// make sure that both sides of the member access are a variable
					if (leftNode->nodeType != ASTnodeType_variable) abort();
					if (rightNode->nodeType != ASTnodeType_variable) {
						addStringToErrorMsg("right side of memberAccess must be a word");
						
						addStringToErrorIndicator("right side of memberAccess is not a word");
						compileError(rightNode->location);
					};
					
					linkedList_Node *leftTypes = NULL;
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, NULL, &leftTypes, data->left, 0, 0, 0);
					
					ContextBinding *structBinding = ((BuilderType *)leftTypes->data)->binding;
					if (structBinding->type != ContextBindingType_struct) abort();
					ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
					
					ASTnode_variable *rightData = (ASTnode_variable *)rightNode->value;
					
					int index = 0;
					
					linkedList_Node *currentMemberName = structData->memberNames;
					linkedList_Node *currentMemberType = structData->memberBindings;
					while (1) {
						if (currentMemberName == NULL) {
							printf("left side of memberAccess must exist\n");
							compileError(((ASTnode *)data->left->data)->location);
						}
						
						SubString *memberName = (SubString *)currentMemberName->data;
						ContextBinding *memberBinding = (ContextBinding *)currentMemberType->data;
						
						if (SubString_SubString_cmp(memberName, rightData->name) == 0) {
							if (memberBinding->type == ContextBindingType_variable) {
								if (types != NULL) {
									addTypeFromBuilderType(GBI, types, &((ContextBinding_variable *)memberBinding->value)->type);
								}
								
								CharAccumulator_appendChars(outerSource, "\n\t%");
								CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
								CharAccumulator_appendChars(outerSource, " = getelementptr inbounds ");
								CharAccumulator_appendChars(outerSource, structData->LLVMname);
								CharAccumulator_appendChars(outerSource, ", ptr ");
								CharAccumulator_appendChars(outerSource, leftInnerSource.data);
								CharAccumulator_appendChars(outerSource, ", i32 0");
								CharAccumulator_appendChars(outerSource, ", i32 ");
								CharAccumulator_appendInt(outerSource, index);
								
								if (loadVariables) {
									CharAccumulator_appendChars(outerSource, "\n\t%");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount + 1);
									CharAccumulator_appendChars(outerSource, " = load ");
									CharAccumulator_appendChars(outerSource, ((ContextBinding_variable *)memberBinding->value)->LLVMtype);
									CharAccumulator_appendChars(outerSource, ", ptr %");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
									CharAccumulator_appendChars(outerSource, ", align ");
									CharAccumulator_appendInt(outerSource, memberBinding->byteAlign);
									
									if (withTypes) {
										CharAccumulator_appendChars(innerSource, ((ContextBinding_variable *)memberBinding->value)->LLVMtype);
										CharAccumulator_appendChars(innerSource, " ");
									}
									
									outerFunction->registerCount++;
								}
								
								CharAccumulator_appendChars(innerSource, "%");
								CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
								
								outerFunction->registerCount++;
							} else if (memberBinding->type == ContextBindingType_function) {
								if (types != NULL) {
									addTypeFromBinding(GBI, types, memberBinding);
								}
//								ContextBinding_function *memberFunction = (ContextBinding_function *)memberBinding->value;
//
//								CharAccumulator_appendChars(innerSource, "@");
//								CharAccumulator_appendChars(innerSource, memberFunction->LLVMname);
							} else {
								abort();
							}
							
							break;
						}
						
						currentMemberName = currentMemberName->next;
						currentMemberType = currentMemberType->next;
						if (memberBinding->type != ContextBindingType_function) {
							index++;
						}
					}
					
					CharAccumulator_free(&leftInnerSource);
					CharAccumulator_free(&rightInnerSource);
					break;
				}
				
				if (expectedTypes == NULL) {
					addStringToErrorMsg("unexpected operator");
					
					addStringToErrorIndicator("the return value of this operator is not used");
					compileError(node->location);
				}
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				char *expectedLLVMtype = getLLVMtypeFromBuilderType(GBI, (BuilderType *)expectedTypes->data);
				
				// all of these operators are very similar and even use the same 'icmp' instruction
				// https://llvm.org/docs/LangRef.html#fcmp-instruction
				if (
					data->operatorType == ASTnode_operatorType_equivalent ||
					data->operatorType == ASTnode_operatorType_greaterThan ||
					data->operatorType == ASTnode_operatorType_lessThan
				) {
					//
					// figure out what the type of both sides should be
					//
					
					buildLLVM(GBI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeftAndRight, data->left, 0, 0, 0);
					if (
						ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int8") &&
						ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int32") &&
						ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int64")
					) {
						if (ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")) {
							printf("operator expected a number\n");
							compileError(node->location);
						}
						
						linkedList_freeList(&expectedTypeForLeftAndRight);
						buildLLVM(GBI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeftAndRight, data->right, 0, 0, 0);
						if (
							ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int8") &&
							ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int32") &&
							ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int64")
						) {
							if (ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")) {
								printf("operator expected a number\n");
								compileError(node->location);
							}
							
							// default to Int32
							linkedList_freeList(&expectedTypeForLeftAndRight);
							addTypeFromString(GBI, &expectedTypeForLeftAndRight, "Int32");
						}
					}
					
					//
					// build both sides
					//
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = icmp ");
					
					if (data->operatorType == ASTnode_operatorType_equivalent) {
						// eq: yields true if the operands are equal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "eq ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_greaterThan) {
						// sgt: interprets the operands as signed values and yields true if op1 is greater than op2.
						CharAccumulator_appendChars(outerSource, "sgt ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_lessThan) {
						// slt: interprets the operands as signed values and yields true if op1 is less than op2.
						CharAccumulator_appendChars(outerSource, "slt ");
					}
					
					else {
						abort();
					}
					
					CharAccumulator_appendChars(outerSource, getLLVMtypeFromBuilderType(GBI, (BuilderType *)expectedTypeForLeftAndRight->data));
				}
				
				// +
				else if (data->operatorType == ASTnode_operatorType_add) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					addTypeFromBuilderType(GBI, &expectedTypeForLeftAndRight, (BuilderType *)expectedTypes->data);
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = add nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				// -
				else if (data->operatorType == ASTnode_operatorType_subtract) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					addTypeFromBuilderType(GBI, &expectedTypeForLeftAndRight, (BuilderType *)expectedTypes->data);
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = sub nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				else {
					abort();
				}
				CharAccumulator_appendChars(outerSource, " ");
				CharAccumulator_appendChars(outerSource, leftInnerSource.data);
				CharAccumulator_appendChars(outerSource, ", ");
				CharAccumulator_appendChars(outerSource, rightInnerSource.data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, expectedLLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				CharAccumulator_appendChars(innerSource, "%");
				CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
				
				CharAccumulator_free(&leftInnerSource);
				CharAccumulator_free(&rightInnerSource);
				
				linkedList_freeList(&expectedTypeForLeftAndRight);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			case ASTnodeType_true: {
				if (ifTypeIsNamed((BuilderType *)expectedTypes->data, "Bool")) {
					printf("unexpected Bool\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "true");
				
				break;
			}

			case ASTnodeType_false: {
				if (ifTypeIsNamed((BuilderType *)expectedTypes->data, "Bool")) {
					printf("unexpected Bool\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "false");
				
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addTypeFromString(GBI, types, "__Number");
					break;
				}
				
				if (
					ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int8") &&
					ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int16") &&
					ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int32") &&
					ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int64")
				) {
					addStringToErrorMsg("unexpected type");
					
					addStringToErrorIndicator("expected type '");
					addSubStringToErrorIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
					addStringToErrorIndicator("' but got a number.");
					compileError(node->location);
				} else {
					ContextBinding *typeBinding = ((BuilderType *)expectedTypes->data)->binding;
					
					if (data->value > pow(2, (typeBinding->byteSize * 8) - 1) - 1) {
						addStringToErrorMsg("integer overflow detected");
						
						CharAccumulator_appendInt(&errorIndicator, data->value);
						addStringToErrorIndicator(" is larger than the maximum size of the type '");
						addSubStringToErrorIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
						addStringToErrorIndicator("'");
						compileError(node->location);
					}
				}
				
				char *LLVMtype = getLLVMtypeFromBuilderType(GBI, (BuilderType *)(*currentExpectedType)->data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, LLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				
				CharAccumulator_appendSubString(innerSource, data->string);
				
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (ifTypeIsNamed((BuilderType *)expectedTypes->data, "Pointer")) {
					addStringToErrorMsg("unexpected type");
					
					addStringToErrorIndicator("expected type '");
					addSubStringToErrorIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToErrorIndicator("' but got a string");
					compileError(node->location);
				}
				
				// + 1 for the NULL byte
				int stringLength = (unsigned int)(data->value->length + 1);
				
				CharAccumulator string = {100, 0, 0};
				CharAccumulator_initialize(&string);
				
				int i = 0;
				int escaped = 0;
				while (i < data->value->length) {
					char character = data->value->start[i];
					
					if (escaped) {
						if (character == '\\') {
							CharAccumulator_appendChars(&string, "\\\\");
							stringLength--;
						} else if (character == 'n') {
							CharAccumulator_appendChars(&string, "\\0A");
							stringLength--;
						} else {
							printf("unexpected character in string after escape '%c'\n", character);
							compileError((SourceLocation){node->location.line, node->location.columnStart + i + 1, node->location.columnEnd - (data->value->length - i)});
						}
						escaped = 0;
					} else {
						if (character == '\\') {
							escaped = 1;
						} else {
							CharAccumulator_appendChar(&string, character);
						}
					}
					
					i++;
				}
				
				CharAccumulator_appendChars(GBI->topLevelConstantSource, "\n@.str.");
				CharAccumulator_appendInt(GBI->topLevelConstantSource, GBI->stringCount);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, " = private unnamed_addr constant [");
				CharAccumulator_appendInt(GBI->topLevelConstantSource, stringLength);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, " x i8] c\"");
				CharAccumulator_appendChars(GBI->topLevelConstantSource, string.data);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, "\\00\""); // the \00 is the NULL byte
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "ptr ");
				}
				CharAccumulator_appendChars(innerSource, "@.str.");
				CharAccumulator_appendInt(innerSource, GBI->stringCount);
				
				GBI->stringCount++;
				
				CharAccumulator_free(&string);
				
				break;
			}
			
			case ASTnodeType_variable: {
				ASTnode_variable *data = (ASTnode_variable *)node->value;
				
				ContextBinding *variableBinding = getContextBindingFromVariableNode(GBI, node);
				if (variableBinding == NULL) {
					addStringToErrorMsg("value not in scope");
					
					addStringToErrorIndicator("nothing named '");
					addSubStringToErrorIndicator(data->name);
					addStringToErrorIndicator("'");
					compileError(node->location);
				}
				if (variableBinding->type == ContextBindingType_variable) {
					ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
					
					if (types != NULL) {
						addTypeFromBuilderType(GBI, types, &variable->type);
					}
					
					if (outerSource != NULL) {
						if (withTypes) {
							CharAccumulator_appendChars(innerSource, variable->LLVMtype);
							CharAccumulator_appendChars(innerSource, " ");
						}
						
						CharAccumulator_appendChars(innerSource, "%");
						
						if (loadVariables) {
							CharAccumulator_appendChars(outerSource, "\n\t%");
							CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
							CharAccumulator_appendChars(outerSource, " = load ");
							CharAccumulator_appendChars(outerSource, variable->LLVMtype);
							CharAccumulator_appendChars(outerSource, ", ptr %");
							CharAccumulator_appendInt(outerSource, variable->LLVMRegister);
							CharAccumulator_appendChars(outerSource, ", align ");
							CharAccumulator_appendInt(outerSource, variableBinding->byteAlign);
							
							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
							
							outerFunction->registerCount++;
						} else {
							CharAccumulator_appendInt(innerSource, variable->LLVMRegister);
						}
					}
				} else if (variableBinding->type == ContextBindingType_function) {
//					ContextBinding_function *function = (ContextBinding_function *)variableBinding->value;
					
					if (types != NULL) {
						addTypeFromBinding(GBI, types, variableBinding);
					}
				} else {
					addStringToErrorMsg("value exists but is not a variable or function");
					
					addStringToErrorIndicator("'");
					addSubStringToErrorIndicator(data->name);
					addStringToErrorIndicator("' is not a variable or function");
					compileError(node->location);
				}
				
				break;
			}
			
			default: {
				printf("unknown node type: %u\n", node->nodeType);
				compileError(node->location);
				break;
			}
		}
		
		if (withCommas && current->next != NULL) {
			CharAccumulator_appendChars(innerSource, ",");
		}
		
		current = current->next;
		if (currentExpectedType != NULL) {
			*currentExpectedType = (*currentExpectedType)->next;
		}
	}
	
	if (GBI->level != 0) {
		linkedList_freeList(&GBI->context[GBI->level]);
	}
	
	GBI->level--;
	
	return hasReturned;
}
