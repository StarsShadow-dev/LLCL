//
// general
//

import { CompileError } from "./report";
import utilities from "./utilities";

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
} | "builtin"

export type CodeGenText = string[] | null

export function getCGText(): string[] {
	return [];
}

//
// tokens
//

export enum TokenType {
	comment,
	
	number,
	string,
	word,
	
	separator,
	operator,
	
	builtinIndicator,
	selfReference,
	singleQuote,
}

export type Token = {
	type: TokenType,
	text: string,
	location: SourceLocation,
	endLine?: number,
}

//
// AST
//

type genericASTnode = {
	location: SourceLocation,
}

export type ASTnode = 

//
// literals
//

genericASTnode & {
	kind: "bool",
	value: boolean,
} | genericASTnode & {
	kind: "number",
	value: number,
} | genericASTnode & {
	kind: "string",
	value: string,
} |

genericASTnode & {
	kind: "identifier",
	name: string,
} | genericASTnode & {
	kind: "call",
	left: ASTnode[],
	callArguments: ASTnode[],
} | genericASTnode & {
	kind: "builtinCall",
	name: string,
	callArguments: ASTnode[],
} | genericASTnode & {
	kind: "operator",
	operatorText: string,
	left: ASTnode[],
	right: ASTnode[],
} | genericASTnode & {
	kind: "comptime",
	value: ASTnode,
} |

//
// structured things (what are these called?)
//

genericASTnode & {
	kind: "definition",
	mutable: boolean,
	isAproperty: boolean,
	name: string,
	type: ASTnode & { kind: "typeUse" } | null,
	value: ASTnode | null,
} | genericASTnode & {
	kind: "function",
	forceInline: boolean,
	functionArguments: ASTnode[],
	returnType: ASTnode & { kind: "typeUse" } | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
	templateType: ASTnode & { kind: "typeUse" } | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "codeGenerate",
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode[],
	trueCodeBlock: ASTnode[],
	falseCodeBlock: ASTnode[] | null,
} | genericASTnode & {
	kind: "return",
	value: ASTnode | null,
} | genericASTnode & {
	kind: "argument",
	name: string,
	type: ASTnode & { kind: "typeUse" },
} | genericASTnode & {
	kind: "typeUse",
	value: ASTnode,
}

//
// scope
//

type genericScopeObject = {
	originLocation: SourceLocation,
}

export type ScopeObject = genericScopeObject & {
	kind: "bool",
	value: boolean,
} | genericScopeObject & {
	kind: "number",
	value: number,
} | genericScopeObject & {
	kind: "string",
	value: string,
} | genericScopeObject & {
	kind: "void",
} | genericScopeObject & {
	kind: "complexValue",
	type: (ScopeObject & { kind: "typeUse" }),
} | genericScopeObject & {
	kind: "alias",
	mutable: boolean,
	isAproperty: boolean,
	name: string,
	value: ScopeObject | null,
	symbolName: string,
	type: (ScopeObject & { kind: "typeUse" }) | null,
} | genericScopeObject & {
	kind: "function",
	forceInline: boolean,
	external: boolean,
	toBeGenerated: boolean,
	indentation: number,
	symbolName: string,
	functionArguments: (ScopeObject & { kind: "argument" })[],
	returnType: (ScopeObject & { kind: "typeUse" }) | null,
	AST: ASTnode[],
	visible: ScopeObject[],
} | genericScopeObject & {
	kind: "argument",
	name: string,
	type: (ScopeObject & { kind: "typeUse" }),
} | genericScopeObject & {
	kind: "struct",
	name: string,
	templateStruct: (ScopeObject & { kind: "struct" }) | null,
	members: ScopeObject[],
} | genericScopeObject & {
	kind: "typeUse",
	comptime: boolean,
	type: ScopeObject,
}

export function unwrapScopeObject(scopeObject: ScopeObject | null): ScopeObject {
	if (scopeObject) {
		if (scopeObject.kind == "alias") {
			if (scopeObject.value) {
				return scopeObject.value;
			} else {
				throw new CompileError(`alias '${scopeObject.name}' used before its definition`)
					.indicator(scopeObject.originLocation, "alias defined here");
			}
		}
		
		return scopeObject;	
	} else {
		throw utilities.unreachable();
	}
}

export function getTypeName(typeUse: ScopeObject & { kind: "typeUse" }): string {
	if (typeUse.type.kind == "struct") {
		return typeUse.type.name;
	} else {
		throw utilities.unreachable();
	}
}